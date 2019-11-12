import getopt
import sys
from analyzer import Analyzer
from z3 import *


# Show help message
def usage():
    pass


# Default command line options
database_file = ""
min_project = 2
verbose = 0

# Deal with command line options
opts, args = getopt.getopt(sys.argv[1:], "hvd:m:")
for op, value in opts:
    if op == "-d":
        database_file = value
    elif op == "-m":
        min_project = int(value)
    elif op == "-v":
        verbose = 1
    elif op == "-h":
        usage()

# Init the analyzer
analyzer = Analyzer(database_file, verbose)

# Get the target functions using the min_project we specified before
target_functions = analyzer.get_target_functions(min_project)

# We don't have to waste time on analyzing these infallible functions
analyzer.set_skip_functions(['strcmp', 'strlen', 'strncmp', 'memcmp', 'strcasecmp', 'strncasecmp','strtol',
                             '__error', '__errno_location', '__ctype_b_loc', '__sync_synchronize','strtoul',
                             'count', 'empty', 'g_strcmp0', 'g_ascii_strcasecmp', 'g_ascii_strncasecmp',
                             'isEmpty', 'isNull', 'qCompare', 'size', 'strchr', 'strstr', 'rand', 'strrchr',
                             'sscanf','snprintf','atoi','fprintf', '_IO_getc'])

# Analyze the similarity of post-branch functions
# Store the result to table function_callee of the database we specified before
#analyzer.postbranch_function_similarity()
analyzer.get_function_action()

# Analyze the equivalence of branch conditions for each target function
# Store the result to column ExprSetID, table condition_equivalence
analyzer.branch_condition_equivalence(target_functions)
