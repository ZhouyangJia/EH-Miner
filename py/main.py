import getopt
import datetime
import sys
import sqlite3

from dummy import Analyzer

opts, args = getopt.getopt(sys.argv[1:], "hd:o:m:")

database_file = ""
min_project = 2


def usage():
    print('Hello World')


for op, value in opts:
    if op == "-d":
        database_file = value
    elif op == "-m":
        min_project = value
    elif op == "-h":
        usage()

analyzer = Analyzer(database_file)

# Get the target functions
target_functions = []
if 0:
    target_functions.append(('BIO_new_file', '/opt/local/include/openssl/bio.h'))
else:
    target_functions = analyzer.get_target_functions(min_project)

# We don't want to waste time for these functions
skip_functions = ['strcmp', 'strlen', 'strncmp', 'memcmp', 'strcasecmp', 'strncasecmp', '__error']

cnt = 0
# Analyze the target functions one by one
for target_function in target_functions:

    call_sites = analyzer.get_call_sites(target_function)
    num = len(call_sites)

    cnt += 1
    now = datetime.datetime.now()
    sys.stdout.write(
        '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(target_functions)))
    sys.stdout.write('Now analyze ' + target_function[0] + '@' + target_function[1] + ' ' + str(num) + '\n')

    sys.stderr.write(
        '%s:%s:%s [%d/%d] ' % (now.hour, now.minute, now.second, cnt, len(target_functions)))
    sys.stderr.write('Now analyze ' + target_function[0] + '@' + target_function[1] + ' ' + str(num) + '\n')

    if target_function[0] in skip_functions:
        continue

    # Init equivalent set
    equal_set_list = []
    # equivalence_map = [[0 for i in range(num)] for j in range(num)]

    for i in range(num):

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

            if index_i != -1 and index_j != -1 and index_i == index_j:
                continue

            is_equivalent = analyzer.get_equivalence(call_sites[i], call_sites[j])

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

                    # if equivalence_map[i][j] == 0:
                    # equivalence_map[i][j] = equivalence_map[j][i] = analyzer.get_equivalence(call_sites[i], call_sites[j])

    # for item in equivalence_map:
    #    sys.stderr.write(str(item) + '\n')


    # Output the result
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
        sys.stdout.write('Non-equivalent set: ')
        for index in non_equivalent_set:
            analyzer.insert_call_site(call_sites[index], 0)
            sys.stdout.write(str(call_sites[index][0]) + ' ')
        sys.stdout.write('\n')

    count = 0
    for item in equal_set_list:
        count += 1
        sys.stdout.write('Equivalent set %d: ' % count)
        for index in item:
            analyzer.insert_call_site(call_sites[index], count)
            sys.stdout.write(str(call_sites[index][0]) + ' ')
        sys.stdout.write('\n')

    analyzer.commit()

