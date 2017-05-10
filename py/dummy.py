import sqlite3
import sys
from z3 import *


class Analyzer(object):
    target_function = []

    s = Solver()
    t = Solver()

    expr_equivalence = {}
    parse_error_set = set()

    def __init__(self, database_file):
        self.conn = sqlite3.connect(database_file)
        stmt = "CREATE TABLE IF NOT EXISTS target_call (ID INTEGER PRIMARY KEY AUTOINCREMENT, BranchID INTEGER, \
              DomainName TEXT, ProjectName TEXT, \
              CallName TEXT, CallDefLoc TEXT, CallID TEXT, CallStr TEXT, CallReturn TEXT, CallSetID INTEGER, \
              ExprStrVec TEXT, PathNumberVec TEXT, \
              LogName TEXT, LogDefLoc TEXT, LogID TEXT, LogStr TEXT)";
        self.conn.execute(stmt)

    def __del__(self):
        self.conn.close()

    def commit(self):
        self.conn.commit()

    def insert_call_site(self, call_site, set_id):

        call_str = call_site[6]
        call_str = call_str.replace('\'', '\'\'')
        expr_str = call_site[12]
        expr_str = expr_str.replace('\'', '\'\'')
        log_str = call_site[19]
        log_str = log_str.replace('\'', '\'\'')

        stmt = "INSERT INTO target_call (BranchID, DomainName, ProjectName, CallName, CallDefLoc, CallID, CallStr, CallReturn, \
            CallSetID, ExprStrVec, PathNumberVec, LogName, LogDefLoc, LogID, LogStr) VALUES \
            ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', %d, '%s', '%s', '%s', '%s', '%s', '%s')" % (
            call_site[0], call_site[1], call_site[2], call_site[3], call_site[4], call_site[5], call_str, call_site[7], set_id,
            expr_str, call_site[13], call_site[16], call_site[17], call_site[18], log_str)
        sys.stderr.write(stmt + '\n')
        self.conn.execute(stmt)

        return

    def get_target_functions(self, min_project):

        target_functions = []

        stmt = "SELECT CallName, CallDefLoc, count(*) FROM call_statistic GROUP BY CallName, CallDefLoc"

        cursor = self.conn.execute(stmt)

        for row in cursor:
            if int(min_project) <= row[2]:
                target_functions.append((row[0], row[1]))

        return target_functions

    def get_call_sites(self, target_function):

        call_sites = []

        stmt = "SELECT * FROM branch_call WHERE CallName = '%s' AND CallDefLoc = '%s'" % \
               (target_function[0], target_function[1])

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

        # Return true if the two expr are exactly the same
        if expr_node_1 == expr_node_2:
            return 1

        # Return the result if being checked before
        expr_pair = expr_node_1 + expr_node_2
        expr_pair.append(call_site_1[3])
        if str(expr_pair) in self.expr_equivalence:
            return self.expr_equivalence[str(expr_pair)]

        # Return false if either of the expr has error when parsing
        if str(expr_node_1) + call_site_1[3] in self.parse_error_set:
            return 0
        if str(expr_node_2) + call_site_2[3] in self.parse_error_set:
            return 0

        # Get the parsed query
        query_1, int_vals_1, real_vals_1, bool_vals_1 = self.get_query(expr_node_1, call_site_1[3])
        query_2, int_vals_2, real_vals_2, bool_vals_2 = self.get_query(expr_node_2, call_site_2[3])

        # Prepare the Z3 stmt
        if 'parse error' in query_1:
            self.parse_error_set.add(str(expr_node_1) + call_site_1[3])
            sys.stderr.write('ID ' + str(call_site_1[0]) + ' ' + query_1 + '\n')
        elif 'parse error' in query_2:
            self.parse_error_set.add(str(expr_node_2) + call_site_2[3])
            sys.stderr.write('ID ' + str(call_site_2[0]) + ' ' + query_2 + '\n')
        else:
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

            query_1_2 = 'And(Not(' + query_1 + '),' + query_2 + ')'
            query_2_1 = 'And(Not(' + query_2 + '),' + query_1 + ')'

            try:
                exec int_val_str
                exec real_val_str
                exec bool_val_str
            except:
                sys.stderr.write('ID ' + str(call_site_1[0]) + ' ' + str(call_site_2[0]) + \
                                 ' declarer wrong: ' + int_val_str + ', ' + real_val_str + ', ' + bool_val_str + '\n')

                expr_pair = expr_node_1 + expr_node_2
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 0
                expr_pair = expr_node_2 + expr_node_1
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 0
                return 0

            self.s.reset()
            self.t.reset()

            try:
                exec 'self.s.add(' + query_1_2 + ')'
                exec 'self.t.add(' + query_2_1 + ')'
            except:
                sys.stderr.write('ID ' + str(call_site_1[0]) + ' ' + str(call_site_2[0]) + \
                                 ' execute wrong: ' + query_1 + ', ' + query_2 + '\n')

                expr_pair = expr_node_1 + expr_node_2
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 0
                expr_pair = expr_node_2 + expr_node_1
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 0
                return 0

            res_s = self.s.check()
            res_t = self.t.check()

            if str(res_s) == 'unsat' and str(res_t) == 'unsat':
                expr_pair = expr_node_1 + expr_node_2
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 1
                expr_pair = expr_node_2 + expr_node_1
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 1
                return 1
            else:
                expr_pair = expr_node_1 + expr_node_2
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 0
                expr_pair = expr_node_2 + expr_node_1
                expr_pair.append(call_site_1[3])
                self.expr_equivalence[str(expr_pair)] = 0
                return 0

        return 0

    @staticmethod
    def get_normalized_expr(call_site):

        # Get call string and return name
        call_str = call_site[6]
        call_ret = call_site[7]

        # Get arguments of the call site
        call_arg_str = call_site[8]
        call_arg = call_arg_str.split('#-_-#')
        if call_arg[0] == '-':
            del call_arg[0]
        if len(call_arg) != call_site[9]:
            sys.stderr.write("ID %d call arg parse error" % (call_site[0]))
            return []

        # Get the expr str and split it into a stack
        expr_node_str = call_site[10]
        expr_node = expr_node_str.split('#-_-#')
        if expr_node[0] == '-':
            del expr_node[0]
        if len(expr_node) != call_site[11]:
            sys.stderr.write("ID %d expr parse error" % (call_site[0]))
            return []

        # Return when the call site is located in a macro
        if len(call_arg) > 0:
            if call_site[3] == call_arg[0]:
                sys.stderr.write("ID %d macro call" % (call_site[0]))
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
                expr_node[i] = call_site[3] + '_0'
                continue

            if expr_node[i] == call_ret:
                expr_node[i] = call_site[3] + '_0'
                continue

            for j in range(len(call_arg)):
                arg = call_arg[j]
                while len(arg) > 1:
                    if arg[0] == '&' or arg[0] == '*':
                        arg = arg[1:]
                    else:
                        break

                if expr_node[i] == arg:
                    expr_node[i] = call_site[3] + '_' + str(j + 1)
                    break

        return expr_node

    @staticmethod
    def get_query(expr_node, call_name):

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
            # print node
            # print mynode
            # print isbool

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
                    mynode.append(right_node + '_' + left_node)
                    isbool.append(False)
                elif 'ARRAY' in node:
                    mynode.append(left_node + '_' + right_node)
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

        # Assign to 'parse error' when an error occurs, or assign to the stack head
        query = 'parse error: ' + errstr if errstr != '' or len(mynode) != 1 else mynode[0]

        return query, int_vals, real_vals, bool_vals
