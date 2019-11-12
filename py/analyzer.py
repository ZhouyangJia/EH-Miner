import sqlite3
import datetime
import sys
import pandas
import random
from z3 import *


class Analyzer(object):

    def __init__(self, database_file, verbose):

        self.verbose = verbose
        self.conn = sqlite3.connect(database_file)
        self.conn.text_factory = str
        self.conn.execute("CREATE INDEX IF NOT EXISTS call1_index ON branch_call(CallName, CallDefLoc)")
        #self.conn.execute("CREATE INDEX IF NOT EXISTS call2_index ON prebranch_call(CallName, CallDefLoc)")
        self.conn.execute("CREATE INDEX IF NOT EXISTS call3_index ON call_statistic(CallName, CallDefLoc)")
        self.conn.execute("CREATE INDEX IF NOT EXISTS call4_index ON function_call(CallName, CallDefLoc)")
        self.conn.execute("CREATE INDEX IF NOT EXISTS func_index ON call_graph(FuncName, FuncDefLoc)")
        #self.conn.execute("CREATE INDEX IF NOT EXISTS log_index ON postbranch_call(LogName, LogDefLoc)")

        self.expr_equivalence = {}
        self.parse_error_set = set()
        self.skip_functions = []

    def __del__(self):
        self.conn.close()

    def progress(self, width, percent):
        print "[%s]%.2f%%\r" % (('%%-%ds' % width) % (width * int(percent)/ 100 * "="), percent),
        if percent >= 100:
           print
        sys.stdout.flush()

    def get_target_functions(self, min_project):
        target_functions = []

        stmt = "SELECT CallName, CallDefLoc, count(*) FROM call_statistic GROUP BY CallName, CallDefLoc"
        cursor = self.conn.execute(stmt)
        for row in cursor:
            if int(min_project) <= row[2]:
                target_functions.append((row[0], row[1]))

        return target_functions

    def import_glibc_return(self):
        glibc_return = pandas.read_csv("glibc_return.csv")
        glibc_return.to_sql("glibc_return", self.conn, if_exists='replace', index=False)
        return

    def set_skip_functions(self, skip_functions):
        self.skip_functions = skip_functions

    def branch_condition_equivalence(self, target_functions):

        # Import the csv file: glibc_return.csv
        self.import_glibc_return()

        # Create the table to store the result
        self.create_condition_equivalence()

        # Analyze the target functions one by one
        cnt = 0
        for target_function in target_functions:

            #if target_function[0] != 'calloc':
            #    continue

            call_sites = self.get_call_sites(target_function)
            num = len(call_sites)

            cnt += 1
            now = datetime.datetime.now()
            sys.stdout.write(
                '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(target_functions)))
            sys.stdout.write('Now analyze branch condition equivalence ' + \
                             target_function[0] + '@' + target_function[1] + ' ' + str(num) + '\n')
            sys.stdout.flush()

            if self.verbose == 1:
                sys.stderr.write(
                    '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(target_functions)))
                sys.stderr.write('Now analyze branch condition equivalence ' + \
                                 target_function[0] + '@' + target_function[1] + ' ' + str(num) + '\n')

            # Skip the functions we don't care about
            if 'operator' in target_function[0] or '__builtin' in target_function[0]:
                continue

            if num == 0:
                continue

            if self.is_skipped(target_function[0]):
                if self.verbose == 1:
                    sys.stderr.write('Skip the function.\n')
                continue

            # Reset the variables
            self.expr_equivalence.clear()
            self.parse_error_set.clear()

            # Init equivalent set
            equal_set_list = []

            for i in range(num):
                self.progress(100, float((i+1)*100)/float(num))
                #myexpr = self.get_normalized_expr(call_sites[i])
                #query, intval, realval, boolval = self.get_query(myexpr, call_sites[i])
                #if query != 'Not(malloc_0)' and query != 'malloc_0==False':
                #    print call_sites[i][0], query
                #    print call_sites[i][7], '=', call_sites[i][6]
                #    print call_sites[i][12], ':', call_sites[i][10]
                #    print myexpr, intval, realval, boolval
                #    print 
                #continue
                
                index_i = -1
                for k in range(len(equal_set_list)):
                    if i in equal_set_list[k]:
                        index_i = k
                        break

                for j in range(i + 1, num):
                    index_j = -1
                    for k in range(len(equal_set_list)):
                        if j in equal_set_list[k]:
                            index_j = k
                            break

                    if index_i >= len(equal_set_list) or index_j >= len(equal_set_list):
                        continue

                    if index_i != -1 and index_j != -1 and index_i == index_j:
                        continue

                    is_equivalent = self.get_equivalence(call_sites[i], call_sites[j])

                    if is_equivalent:
                        if index_i != -1 and index_j != -1 and index_i != index_j:
                            equal_set_list[index_i] = equal_set_list[index_i] | equal_set_list[index_j]
                            del equal_set_list[index_j]

                        elif index_i == -1 and index_j == -1:
                            myset = set()
                            myset.add(i)
                            myset.add(j)
                            equal_set_list.append(myset)

                        elif index_i == -1 and index_j != -1:
                            equal_set_list[index_j].add(i)

                        elif index_i != -1 and index_j == -1:
                            equal_set_list[index_i].add(j)

            # Store the result
            non_equivalent_set = set()
            for i in range(num):
                index_i = -1
                for k in range(len(equal_set_list)):
                    if i in equal_set_list[k]:
                        index_i = k
                        break
                if index_i == -1:
                    non_equivalent_set.add(i)

            if len(non_equivalent_set) > 0:
                if self.verbose == 1:
                    sys.stderr.write('Non-equivalent set: ')
                for index in non_equivalent_set:
                    self.insert_condition_equivalence(call_sites[index], 0, 'UNKNOWN')
                    if self.verbose == 1:
                        sys.stderr.write(str(call_sites[index][0]) + ' ')
                if self.verbose == 1:
                    sys.stderr.write('\n')

            stmt = "SELECT * from glibc_return WHERE CallName = '%s' AND CallDefLoc = '%s'" % (
                call_sites[0][3], call_sites[0][4])
            cursor = self.conn.execute(stmt)
            glibc_return = []
            for row in cursor:
                glibc_return = row
                break

            count = 0
            for equal_set in equal_set_list:
                count += 1
                if self.verbose == 1:
                    sys.stderr.write('Equivalent set %d: ' % count)

                path_intention = 'UNCHECK'
                flag = True
                for index in equal_set:
                    if flag and len(glibc_return) > 0:
                        path_intention = self.get_path_intention(call_sites[index], \
                                                                 glibc_return[3], glibc_return[4], glibc_return[5])
                        flag = False
                        #print path_intention

                    self.insert_condition_equivalence(call_sites[index], count, path_intention)
                    if self.verbose == 1:
                        sys.stderr.write(str(call_sites[index][0]) + ' ')
                if self.verbose == 1:
                    sys.stderr.write('\n')

    def get_path_intention(self, call_site, return_type, normal_query, error_query):

        expr_node = self.get_normalized_expr(call_site)
        branch_query, int_vals, real_vals, bool_vals = self.get_query(expr_node, call_site)

        return_query = 'Or(' + normal_query + ', ' + error_query + ')'
        branch_query = 'And(' + branch_query + ', ' + return_query + ')'

        if return_type == 'INT':
            int_vals.add(call_site[3]+'_0')
        elif return_type == 'POINTER':
            bool_vals.add(call_site[3]+'_0')

        int_vals.add('dummy_int_1')
        int_vals.add('dummy_int_2')
        int_val_str = ''
        int_val_blank = ''
        for int_var in int_vals:
            int_val_str += int_var + ','
            int_val_blank += int_var + ' '
        int_val_str = int_val_str[:-1]
        int_val_blank = int_val_blank[:-1]
        int_val_str = int_val_str + '=Ints(\'' + int_val_blank + '\')'

        real_vals.add('dummy_real_1')
        real_vals.add('dummy_real_2')
        real_val_str = ''
        real_val_blank = ''
        for real_var in real_vals:
            real_val_str += real_var + ','
            real_val_blank += real_var + ' '
        real_val_str = real_val_str[:-1]
        real_val_blank = real_val_blank[:-1]
        real_val_str = real_val_str + '=Reals(\'' + real_val_blank + '\')'

        bool_vals.add('dummy_bool_1')
        bool_vals.add('dummy_bool_2')
        bool_val_str = ''
        bool_val_blank = ''
        for bool_var in bool_vals:
            bool_val_str += bool_var + ','
            bool_val_blank += bool_var + ' '
        bool_val_str = bool_val_str[:-1]
        bool_val_blank = bool_val_blank[:-1]
        bool_val_str = bool_val_str + '=Bools(\'' + bool_val_blank + '\')'

        query_branch_normal = 'And(Not(' + branch_query + '),' + normal_query + ')'
        query_normal_branch = 'And(Not(' + normal_query + '),' + branch_query + ')'

        query_branch_error = 'And(Not(' + branch_query + '),' + error_query + ')'
        query_error_branch = 'And(Not(' + error_query + '),' + branch_query + ')'

        #print query_branch_normal, query_normal_branch
        #print query_branch_error, query_error_branch
        try:
            exec int_val_str
            exec real_val_str
            exec bool_val_str

            s = Solver()
            t = Solver()

            exec 's.add(' + query_branch_normal + ')'
            exec 't.add(' + query_normal_branch + ')'

            res_s = s.check()
            res_t = t.check()
            if str(res_s) == 'unsat' and str(res_t) == 'unsat':
                return 'NORMAL'
            elif str(res_s) == 'sat' and str(res_t) == 'unsat':
                return 'SUB-NORMAL'

            s.reset()
            t.reset()

            exec 's.add(' + query_branch_error + ')'
            exec 't.add(' + query_error_branch + ')'

            res_s = s.check()
            res_t = t.check()

            if str(res_s) == 'unsat' and str(res_t) == 'unsat':
                return 'ERROR'
            elif str(res_s) == 'sat' and str(res_t) == 'unsat':
                return 'SUB-ERROR'

        except:
            return 'UNKNOWN'

        return 'UNKNOWN'

    def is_skipped(self, call_name):
        skip_flag = 0
        for skip_function in self.skip_functions:
            if skip_function == call_name:
                skip_flag = 1
                break
        return skip_flag

    def create_condition_equivalence(self):

        stmt = "DROP TABLE IF EXISTS condition_equivalence"
        self.conn.execute(stmt)
        stmt = "CREATE TABLE condition_equivalence (ID INTEGER PRIMARY KEY AUTOINCREMENT, BranchID INTEGER, \
              DomainName TEXT, ProjectName TEXT, \
              CallName TEXT, CallDefLoc TEXT, CallID TEXT, CallStr TEXT, CallReturn TEXT, ExprSetID INTEGER, \
              PathIntention TEXT, ExprStrVec TEXT, PathNumberVec TEXT, \
              LogName TEXT, LogDefLoc TEXT, LogID TEXT, LogStr TEXT)"
        self.conn.execute(stmt)

        self.conn.execute("CREATE INDEX IF NOT EXISTS call4_index ON condition_equivalence(CallName, CallDefLoc)")

    def insert_condition_equivalence(self, call_site, set_id, path_intention):

        call_str = call_site[6]
        call_str = call_str.replace('\'', '\'\'')
        expr_str = call_site[12]
        expr_str = expr_str.replace('\'', '\'\'')
        log_str = call_site[19]
        log_str = log_str.replace('\'', '\'\'')

        stmt = "INSERT INTO condition_equivalence (BranchID, DomainName, ProjectName, \
            CallName, CallDefLoc, CallID, CallStr, CallReturn, \
            ExprSetID, PathIntention, ExprStrVec, PathNumberVec, LogName, LogDefLoc, LogID, LogStr) VALUES \
            ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', %d, '%s', '%s', '%s', '%s', '%s', '%s', '%s')" % (
            call_site[0], call_site[1], call_site[2], call_site[3], call_site[4], call_site[5], call_str, call_site[7],
            set_id, path_intention, expr_str, call_site[13], call_site[16], call_site[17], call_site[18], log_str)
        self.conn.execute(stmt)
        self.conn.commit()

    def get_call_sites(self, target_function):

        call_sites = []
        stmt = "SELECT * FROM branch_call WHERE CallName = '%s' AND CallDefLoc = '%s' AND \
            LogName in (select LogName from function_action)" % (target_function[0], target_function[1])
        cursor = self.conn.execute(stmt)
        for row in cursor:
            call_sites.append(row)

        return call_sites

    def get_equivalence(self, call_site_1, call_site_2):

        expr_node_1 = self.get_normalized_expr(call_site_1)
        expr_node_2 = self.get_normalized_expr(call_site_2)

        # Return false if either of the expr is empty
        if expr_node_1 == [] or expr_node_2 == []:
            return 0

        # Return false if either of the expr has error when parsing
        if str(expr_node_1) + call_site_1[3] in self.parse_error_set:
            return 0
        if str(expr_node_2) + call_site_2[3] in self.parse_error_set:
            return 0

        # Get the parsed query
        query_1, int_vals_1, real_vals_1, bool_vals_1 = self.get_query(expr_node_1, call_site_1)
        query_2, int_vals_2, real_vals_2, bool_vals_2 = self.get_query(expr_node_2, call_site_2)

        # Return the result if being checked before
        query_pair = query_1 + query_2 + call_site_1[3]
        if str(query_pair) in self.expr_equivalence:
            return self.expr_equivalence[str(query_pair)]

        # Prepare the Z3 stmt
        if 'parse error' in query_1:
            self.parse_error_set.add(str(expr_node_1) + call_site_1[3])
            if self.verbose == 1:
                sys.stderr.write('ID ' + str(call_site_1[0]) + ' ' + query_1 + '\n')
        elif 'parse error' in query_2:
            self.parse_error_set.add(str(expr_node_2) + call_site_2[3])
            if self.verbose == 1:
                sys.stderr.write('ID ' + str(call_site_2[0]) + ' ' + query_2 + '\n')
        else:

            query_1_new = query_1
            query_2_new = query_2

            stmt = "SELECT * from glibc_return WHERE CallName = '%s' AND CallDefLoc = '%s'" % (
            call_site_1[3], call_site_1[4])
            cursor = self.conn.execute(stmt)
            for row in cursor:
                if row[3] == 'INT':
                    int_vals_1.add(row[1]+'_0')
                elif row[3] == 'POINTER':
                    bool_vals_1.add(row[1]+'_0')

                return_query = 'Or(' + row[4] + ', ' + row[5] + ')'
                query_1_new = 'And(' + query_1 + ', ' + return_query + ')'
                query_2_new = 'And(' + query_2 + ', ' + return_query + ')'
                # print query_1, query_2
                # print query_1_new, query_2_new
                break

            int_vals = int_vals_1 | int_vals_2
            int_vals.add('dummy_int_1')
            int_vals.add('dummy_int_2')
            int_val_str = ''
            int_val_blank = ''
            for int_var in int_vals:
                int_val_str += int_var + ','
                int_val_blank += int_var + ' '
            int_val_str = int_val_str[:-1]
            int_val_blank = int_val_blank[:-1]
            int_val_str = int_val_str + '=Ints(\'' + int_val_blank + '\')'

            real_vals = real_vals_1 | real_vals_2
            real_vals.add('dummy_real_1')
            real_vals.add('dummy_real_2')
            real_val_str = ''
            real_val_blank = ''
            for real_var in real_vals:
                real_val_str += real_var + ','
                real_val_blank += real_var + ' '
            real_val_str = real_val_str[:-1]
            real_val_blank = real_val_blank[:-1]
            real_val_str = real_val_str + '=Reals(\'' + real_val_blank + '\')'

            bool_vals = bool_vals_1 | bool_vals_2
            bool_vals.add('dummy_bool_1')
            bool_vals.add('dummy_bool_2')
            bool_val_str = ''
            bool_val_blank = ''
            for bool_var in bool_vals:
                bool_val_str += bool_var + ','
                bool_val_blank += bool_var + ' '
            bool_val_str = bool_val_str[:-1]
            bool_val_blank = bool_val_blank[:-1]
            bool_val_str = bool_val_str + '=Bools(\'' + bool_val_blank + '\')'

            query_1_2 = 'And(Not(' + query_1_new + '),' + query_2_new + ')'
            query_2_1 = 'And(Not(' + query_2_new + '),' + query_1_new + ')'


            try:
                exec int_val_str
                exec real_val_str
                exec bool_val_str
            except:

                if self.verbose == 1:
                    sys.stderr.write('ID ' + str(call_site_1[0]) + ' ' + str(call_site_2[0]) + \
                        ' declare wrong: ' + int_val_str + ', ' + real_val_str + ', ' + bool_val_str + '\n')

                query_pair = query_1 + query_2 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 0
                query_pair = query_2 + query_1 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 0
                return 0

            s = Solver()
            t = Solver()

            try:
                exec 's.add(' + query_1_2 + ')'
                exec 't.add(' + query_2_1 + ')'
            except:

                if self.verbose == 1:
                    sys.stderr.write('ID ' + str(call_site_1[0]) + ' ' + str(call_site_2[0]) + \
                                     ' execute wrong: ' + query_1_new + ', ' + query_2_new + ',' + str(bool_vals) \
                                     + ',' + str(int_vals) + ',' + str(real_vals) + '\n')

                query_pair = query_1 + query_2 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 0
                query_pair = query_2 + query_1 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 0
                return 0

            res_s = s.check()
            res_t = t.check()

            if str(res_s) == 'unsat' and str(res_t) == 'unsat':
                query_pair = query_1 + query_2 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 1
                query_pair = query_2 + query_1 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 1
                return 1
            else:
                query_pair = query_1 + query_2 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 0
                query_pair = query_2 + query_1 + call_site_1[3]
                self.expr_equivalence[str(query_pair)] = 0
                return 0

        return 0

    def get_normalized_expr(self, call_site):

        # Get call name, call string and return name
        call_name = call_site[3]
        call_str = call_site[6]
        callret = call_site[7]
        while len(callret) > 1:
            if callret[0] == '&' or callret[0] == '*':
                callret = callret[1:]
            else:
                break
        call_ret = callret.split('#-_-#')

        # Get arguments of the call site
        call_arg_str = call_site[8]
        call_arg = call_arg_str.split('#-_-#')
        if call_arg[0] == '-':
            del call_arg[0]
        if len(call_arg) != int(call_site[9]):
            if self.verbose == 1:
                sys.stderr.write("ID %d call arg parse error" % (call_site[0]))
            return []
        for j in range(len(call_arg)):
            while len(call_arg[j]) > 1:
                if call_arg[j][0] == '&' or call_arg[j][0] == '*':
                    call_arg[j] = call_arg[j][1:]
                else:
                    break

        # Get the expr str and split it into a stack
        expr_node_str = call_site[10]
        expr_node = expr_node_str.split('#-_-#')
        if expr_node[0] == '-':
            del expr_node[0]
        if len(expr_node) != int(call_site[11]):
            if self.verbose == 1:
                sys.stderr.write("ID %d expr parse error" % (call_site[0]))
            return []

        # Perform name normalization
        for i in range(len(expr_node)):

            if expr_node[i].isdigit():
                continue

            is_float = False
            try:
                float(expr_node[i])
            except:
                is_float = False
            else:
                is_float = True
            if is_float:
                continue

            if expr_node[i] == call_str:
                expr_node[i] = call_name + '_0'
                continue

            flag = 0
            for ret in call_ret:
                if expr_node[i] == ret:
                    expr_node[i] = call_name + '_0'
                    flag = 1
                    break
            if flag == 1:
                continue

            for j in range(len(call_arg)):
                arg = call_arg[j]
                if expr_node[i] == arg:
                    expr_node[i] = call_name + '_' + str(j + 1)
                    break

        return expr_node

    def get_query(self, expr_node, call_site):

        call_name = call_site[3]

        callret = call_site[7]
        callret = callret.replace('.', '_')
        callret = callret.replace('->', '_')
        callret = callret.replace('[', '_')
        callret = callret.replace(']', '_')
        while len(callret) > 1:
            if callret[0] == '&' or callret[0] == '*':
                callret = callret[1:]
            else:
                break
        call_ret = callret.split('#-_-#')


        # Get arguments of the call site
        call_arg_str = call_site[8]
        call_arg = call_arg_str.split('#-_-#')
        if call_arg[0] == '-':
            del call_arg[0]
        if len(call_arg) != int(call_site[9]):
            if self.verbose == 1:
                sys.stderr.write("ID %d call arg parse error" % (call_site[0]))
            return []
        for j in range(len(call_arg)):
            call_arg[j] = call_arg[j].replace('.', '_')
            call_arg[j] = call_arg[j].replace('->', '_')
            call_arg[j] = call_arg[j].replace('[', '_')
            call_arg[j] = call_arg[j].replace(']', '_')
            while len(call_arg[j]) > 1:
                if call_arg[j][0] == '&' or call_arg[j][0] == '*':
                    call_arg[j] = call_arg[j][1:]
                else:
                    break

        # Init variables
        int_vals = set()
        real_vals = set()
        bool_vals = set()

        # Record the temp node and its abstract bool type when parsing
        mynode = []
        isbool = []

        # Parse the node stack
        errstr = ''
        for node in expr_node:

            is_float = False
            try:
                float(node)
            except:
                is_float = False
            else:
                is_float = True

            if is_float:
                mynode.append(node)
                isbool.append(False)
            elif call_name + '_' in node:
                mynode.append(node)
                isbool.append(False)
            elif node == ':?':
                errstr = '?: operator'
                break

            elif 'BO_' in node:
                if len(mynode) < 2:
                    errstr = 'wrong tree'
                    break

                right_node = mynode.pop()
                right_bool = isbool.pop()

                left_node = mynode.pop()
                left_bool = isbool.pop()

                if '2_*' in node:
                    mynode.append(left_node + '*' + right_node)
                    isbool.append(False)
                elif '3_/' in node:
                    mynode.append(left_node + '/' + right_node)
                    isbool.append(False)
                elif '4_%' in node:
                    mynode.append(left_node + '%' + right_node)
                    isbool.append(False)
                elif '5_+' in node:
                    mynode.append(left_node + '+' + right_node)
                    isbool.append(False)
                elif '6_-' in node:
                    mynode.append(left_node + '-' + right_node)
                    isbool.append(False)
                elif '9_<' in node:
                    mynode.append(left_node + '<' + right_node)
                    isbool.append(True)
                elif '10_>' in node:
                    mynode.append(left_node + '>' + right_node)
                    isbool.append(True)
                elif '11_<=' in node:
                    mynode.append(left_node + '<=' + right_node)
                    isbool.append(True)
                elif '12_>=' in node:
                    mynode.append(left_node + '>=' + right_node)
                    isbool.append(True)
                elif '13_==' in node:
                    if left_bool and right_node.isdigit():
                        right_node = 'False' if right_node == '0' else 'True'
                    if right_bool and left_node.isdigit():
                        left_node = 'False' if left_node == '0' else 'True'
                    mynode.append(left_node + '==' + right_node)
                    isbool.append(True)
                elif '14_!=' in node:
                    if left_bool and right_node.isdigit():
                        right_node = 'False' if right_node == '0' else 'True'
                    if right_bool and left_node.isdigit():
                        left_node = 'False' if left_node == '0' else 'True'
                    mynode.append(left_node + '!=' + right_node)
                    isbool.append(True)
                elif '18_&&' in node:
                    if not left_bool:
                        left_node = left_node + '!=0'
                    if not right_bool:
                        right_node = right_node + '!=0'
                    mynode.append('And(' + left_node + ',' + right_node + ')')
                    isbool.append(True)
                elif '19_||' in node:
                    if not left_bool:
                        left_node = left_node + '!=0'
                    if not right_bool:
                        right_node = right_node + '!=0'
                    mynode.append('Or(' + left_node + ',' + right_node + ')')
                    isbool.append(True)
                elif '20_=' in node:
                    mynode.append(right_node)
                    isbool.append(right_bool)
                elif 'MEMBER' in node:
                    variable = right_node + '_' + left_node
                    for ret in call_ret:
                        if variable == ret:
                            variable = call_name + '_0'
                            break
                    for i in range(len(call_arg)):
                        if variable == call_arg[i]:
                            variable = call_name + '_' + str(i + 1)
                            break
                    mynode.append(variable)
                    isbool.append(False)
                elif 'ARRAY' in node:
                    variable = left_node + '_' + right_node + '_'
                    for ret in call_ret:
                        if variable == ret:
                            variable = call_name + '_0'
                            break
                    for i in range(len(call_arg)):
                        if variable == call_arg[i]:
                            variable = call_name + '_' + str(i + 1)
                            break
                    mynode.append(variable)
                    isbool.append(False)
                else:
                    errstr = 'unsupport binary operator: ' + node
                    break

            elif 'UO_' in node:
                if len(mynode) < 1:
                    errstr = 'wrong tree'
                    break

                child_node = mynode.pop()
                child_bool = isbool.pop()

                if '6_+' in node:
                    mynode.append('+' + child_node)
                    isbool.append(False)
                elif '7_-' in node:
                    mynode.append('-' + child_node)
                    isbool.append(False)
                elif '9_!' in node:
                    if not child_bool:
                        child_node = child_node + '!=0'
                    mynode.append('Not(' + child_node + ')')
                    isbool.append(True)
                elif 'VARIABLE_INT' in node:
                    mynode.append(child_node)
                    isbool.append(False)
                    if child_node.startswith(call_name + '_'):
                        int_vals.add(child_node)
                elif 'VARIABLE_BOOL' in node:
                    mynode.append(child_node)
                    isbool.append(True)
                    if child_node.startswith(call_name + '_'):
                        bool_vals.add(child_node)
                elif 'VARIABLE_FLOAT' in node:
                    mynode.append(child_node)
                    isbool.append(False)
                    if child_node.startswith(call_name + '_'):
                        real_vals.add(child_node)
                elif 'VARIABLE_POINTER' in node:
                    mynode.append(child_node)
                    isbool.append(True)
                    if child_node.startswith(call_name + '_'):
                        bool_vals.add(child_node)
                else:
                    mynode.append(child_node)
                    isbool.append(False)

            else:
                mynode.append(node)
                isbool.append(False)

        if len(isbool) == 1 and not isbool[0]:
            mynode[0] += '!=0'
            isbool[0] = True

        # Assign 'parse error' when an error occurs, or assign the stack head
        query = 'parse error: ' + errstr if errstr != '' or len(mynode) != 1 else mynode[0]

        return query, int_vals, real_vals, bool_vals

    def postbranch_function_similarity(self):

        postbranch_function_set = set()
        stmt = "SELECT distinct LogName, LogDefLoc FROM condition_equivalence WHERE ExprSetID != 0"
        cursor = self.conn.execute(stmt)
        for row in cursor:
            postbranch_function_set.add((row[0], row[1]))

        # Create the table to store the result
        self.create_function_similarity()

        self.insert_function_similarity(('return', '-'), ('return', 1.0))

        cnt = 0
        for postbranch_function in postbranch_function_set:
            cnt += 1
            now = datetime.datetime.now()

            sys.stdout.write(
                '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(postbranch_function_set)))
            sys.stdout.write('Now analyze postbranch function similarity ' + \
                             postbranch_function[0] + '@' + postbranch_function[1] + '\n')

            if self.verbose == 1:
                sys.stderr.write(
                    '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(postbranch_function_set)))
                sys.stderr.write('Now analyze postbranch function similarity ' + \
                                 postbranch_function[0] + '@' + postbranch_function[1] + '\n')

            callee_functions = {}
            callee_functions[(postbranch_function[0], postbranch_function[1])] = 1
            root_functions = set()

            root_functions.add(postbranch_function)
            weight = 1.0
            while len(root_functions) > 0 and weight > 0.05:  # 0.05 > 1/(2^5). 3 levels: 0-4
                weight /= 2.0

                next_root_functions = set()
                for root_function in root_functions:
                    stmt = "select distinct CallName, CallDefLoc from call_graph \
                          where FuncName = '%s' and FuncDefLoc = '%s'" % (root_function[0], root_function[1])
                    cursor = self.conn.execute(stmt)
                    for row in cursor:
                        next_root_functions.add((row[0], row[1]))

                for next_root_function in next_root_functions:
                    if not next_root_function in callee_functions:
                        callee_functions[next_root_function] = weight
                    else:
                        callee_functions[next_root_function] += weight
                root_functions = next_root_functions

            intent_weights = self.get_intent_weights(callee_functions)
            for intent_weight in intent_weights:
                if intent_weight[1] > 0.0:
                    self.insert_function_similarity(postbranch_function, intent_weight)

    def create_function_similarity(self):

        stmt = "DROP TABLE IF EXISTS function_similarity"
        self.conn.execute(stmt)
        stmt = "CREATE TABLE function_similarity (ID INTEGER PRIMARY KEY AUTOINCREMENT, LogName TEXT, LogDefLoc TEXT, \
              Intention TEXT, Weight REAL)"
        self.conn.execute(stmt)

        self.conn.execute("CREATE INDEX IF NOT EXISTS log2_index ON function_similarity(LogName, LogDefLoc)")

    def insert_function_similarity(self, postbranch_function, intent_weight):

        stmt = "INSERT INTO function_similarity (LogName, LogDefLoc, Intention, Weight) VALUES ('%s', '%s', '%s', %f)" \
               % (postbranch_function[0], postbranch_function[1], intent_weight[0], intent_weight[1])
        self.conn.execute(stmt)
        self.conn.commit()

    def get_intent_weights(self, callee_functions):

        #alloc_functions = ['malloc', 'alloca', 'calloc', 'realloc', 'reallocf', 'valloc']

        #resource_functions = ['getpriority', 'getrlimit', 'getrusage', 'setpriority', 'setrlimit', \
                              #'sysconf', 'pathconf', 'fpathconf']

        #exec_functions = ['system', 'popen', 'execl', 'execle', 'execlp', 'execv', 'execve', 'execvp']

        #time_functions = ['localtime', 'asctime', 'ctime', 'difftime', 'gmtime', 'time', 'tzset']

        #signal_functions = ['kill', 'killpg', 'raise', 'alarm', 'setitimer', 'pthread_kill', 'signal', 'bsd_signal', \
                            #'sigaction', 'sigblock', 'sigwait', 'sigset', 'sigpause', 'sigpending', 'siginterrupt',
                            #'sigaddset']

        exit_functions = ['abort', 'exit', 'kill', 'killpg', 'raise', 'alarm', 'signal']

        exit_keywords = ['abort', 'exit', 'die', 'kill']

        output_functions = ['printf', 'fprintf', 'dprintf', 'vprintf', 'vfprintf', 'vdprintf', \
                            'fputs', 'puts', 'fwrite', 'perror', 'psignal', 'psiginfo', 'syslog', \
                            'pwrite', 'write', 'writev', 'written', 'msgsnd', 'send', 'sendto', 'sendmsg']

        output_keywords = [
            "error","err","warn","alert","assert","fail","crit","emerg","out","exit","die","halt","suspend",
            "wrong","fatal","fault","misplay","damage","illegal","exception","errmsg","abort","msg","record","report",
            "stop","quit","close","put","print","write","log","message","dump","hint","trace","notify"]


        free_functions = ['free']

        free_keywords = ['free', 'clean', 'clear']

        delete_functions = ['remove', 'unlink', 'unlinkat', 'rmdir']

        delete_keywords = ['rm', 'unlink', 'del', 'clean']

        close_functions = ['close', 'fclose', 'pclose', 'shutdown', 'closelog']

        close_keywords = ['close', 'shutdown']

        return_functions = ['return']

        return_keywords = ['return']

        functions_list = [exit_functions, output_functions, free_functions, delete_functions, close_functions, return_functions]

        keywords_list = [exit_keywords, output_keywords, free_keywords, delete_keywords, close_keywords, return_keywords]

        functions_name = ['exit', 'output', 'free', 'delete', 'close', 'return']

        ret = []
        for i in range(len(functions_list)):
            intent_functions = functions_list[i]
            intent_weight = 0.0
            for intent_function in intent_functions:
                for callee_function, callee_weight in callee_functions.items():
                    if callee_function[0] == intent_function:
                        intent_weight += callee_weight
            ret.append((functions_name[i], intent_weight))

        return ret

    def get_function_action(self):
        postbranch_function_set = set()
        stmt = "SELECT distinct LogName, LogDefLoc FROM branch_call"
        cursor = self.conn.execute(stmt)
        for row in cursor:
            postbranch_function_set.add((row[0], row[1]))

        # Create the table to store the result
        self.create_function_action()

        exit_functions = ['abort', 'exit', 'kill', 'killpg', 'raise', 'alarm', 'signal']
        exit_keywords = ['abort', 'exit', 'die', 'kill', 'quit', 'stop']

        output_functions = ['printf', 'fprintf', 'dprintf', 'vprintf', 'vfprintf', 'vdprintf', \
                            'fputs', 'puts', 'fwrite', 'perror', 'psignal', 'psiginfo', 'syslog', \
                            'pwrite', 'write', 'writev', 'written', 'msgsnd', 'send', 'sendto', 'sendmsg']
        output_keywords = ["error", "err", "warn", "alert", "assert", "fail", "crit", "emerg", "out", "exit", "die", "halt",
            "suspend", "wrong", "fatal", "fault", "misplay", "damage", "illegal", "exception", "errmsg", "abort", "msg",
            "record", "report", "stop", "quit", "close", "put", "print", "write", "log", "message", "dump", "hint", "trace", "notify"]

        free_functions = ['free']
        free_keywords = ['free', 'clean', 'clear']

        delete_functions = ['remove', 'unlink', 'unlinkat', 'rmdir']
        delete_keywords = ['rm', 'unlink', 'del', 'clean']

        close_functions = ['close', 'fclose', 'pclose', 'shutdown', 'closelog']
        close_keywords = ['close', 'shutdown']

        return_functions = ['return']
        return_keywords = ['return']
        goto_functions = ['goto']
        goto_keywords = ['goto']
        break_functions = ['break']
        break_keywords = ['break']
        continue_functions = ['continue']
        continue_keywords = ['continue']

        functions_list = [exit_functions, output_functions, free_functions, delete_functions, close_functions,
                          return_functions, goto_functions, break_functions, continue_functions]

        keywords_list = [exit_keywords, output_keywords, free_keywords, delete_keywords, close_keywords,
                         return_keywords, goto_keywords, break_keywords, continue_keywords]

        action_name = ['exit', 'output', 'free', 'delete', 'close', 'return', 'goto', 'break', 'continue']

        cnt = 0
        for postbranch_function in postbranch_function_set:
            cnt += 1
            now = datetime.datetime.now()
            '''
            sys.stdout.write(
                '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(postbranch_function_set)))
            sys.stdout.write('Now analyze postbranch function action ' + \
                             postbranch_function[0] + '@' + postbranch_function[1] + '\n')
            '''
            if self.verbose == 1:
                sys.stderr.write(
                    '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(postbranch_function_set)))
                sys.stderr.write('Now analyze postbranch function action ' + \
                                 postbranch_function[0] + '@' + postbranch_function[1] + '\n')

            parent = {}
            for i in range(len(action_name)):
                root_functions = set()
                root_functions.add(postbranch_function)
                count = 0
                parent.clear()
                parent[postbranch_function[0]] = '__TOP__'

                while len(root_functions) > 0 and count < 20:
                    count += 1

                    next_root_functions = set()
                    for root_function in root_functions:

                        is_action_function = 0
                        for function in functions_list[i]:
                            if function == root_function[0]:
                                is_action_function = 1

                                trace_function = root_function[0]
                                trace_str = trace_function
                                level = 1
                                function_map = {}
                                function_map.clear()
                                function_map[trace_function] = 1
                                while parent.get(trace_function) != '__TOP__':
                                    trace_function = parent.get(trace_function)
                                    if function_map.get(trace_function) == 1:
                                        break
                                    function_map[trace_function] = 1
                                    trace_str = trace_function + '->' + trace_str
                                    level += 1

                                trace_str = trace_str.replace('\'', '\'\'')
                                self.insert_function_action(postbranch_function, action_name[i], trace_str, level)
                                break
                        if is_action_function == 1:
                            count = 20
                            break

                        has_action_keyword = 0
                        for keyword in keywords_list[i]:
                            if keyword in root_function[0].lower():
                                has_action_keyword = 1
                                break
                        if has_action_keyword == 1:
                            stmt = "select distinct CallName, CallDefLoc from call_graph \
                                  where FuncName = '%s' and FuncDefLoc = '%s'" % (root_function[0], root_function[1])
                            cursor = self.conn.execute(stmt)
                            for row in cursor:
                                child_function = (row[0], row[1])
                                next_root_functions.add(child_function)
                                parent[child_function[0]] = root_function[0]

                    root_functions = next_root_functions



    def create_function_action(self):
        stmt = "DROP TABLE IF EXISTS function_action"
        self.conn.execute(stmt)
        stmt = "CREATE TABLE function_action (ID INTEGER PRIMARY KEY AUTOINCREMENT, LogName TEXT, LogDefLoc TEXT,\
              Intention TEXT, Trace TEXT, Level INTEGER)"
        self.conn.execute(stmt)

        self.conn.execute("CREATE INDEX IF NOT EXISTS log3_index ON function_action(LogName, LogDefLoc)")

    def insert_function_action(self, postbranch_function, action, trace, level):
        stmt = "INSERT INTO function_action (LogName, LogDefLoc, Intention, Trace, Level) VALUES ('%s', '%s', '%s', '%s', '%d')" \
               % (postbranch_function[0], postbranch_function[1], action, trace, level)
        self.conn.execute(stmt)
        self.conn.commit()
