# EH-Miner
Detecting Error-Handling Bugs without Error Specification Input

---

### Introduction
EH-Miner is a bug detection tool for error-handling bugs. Existing tools of detecting error-handling bugs require error specifications as input. Manually generating error specifications is error-prone and tedious. The accuracy of automatically mined error specifications is inadequate. We design EH-Miner, a novel and practical tool that can automatically detect error-handling bugs without the need for error specifications.


### Usage

##### Compile
- Change directory to Clang tools directory (recommend llvm-5.0.0).

```
cd path/to/llvm-source-tree/tools/clang/tools
```

- Download source code.

```
git clone https://github.com/ZhouyangJia/EH-Miner.git
```

- Add *add_subdirectory(clang-ehminer)* to CMakeList.txt.

```
echo "add_subdirectory(clang-ehminer)" >> CMakeLists.txt
```

- Install dependenceis

```
sudo apt install libconfig-dev libsqlite3-dev
```

- If the libraries are not installed in standard paths, in clang-ehminer/CMakeList.txt, change the path: 

```
include_directories("/path/to/include")
link_directories("/path/to/lib")
```

- [Recompile Clang](http://llvm.org/docs/CMake.html) and try following command to test.

```
export $PATH=$PATH:/path/to/llvm/build/bin
clang-ehminer -help
```


- Copy the py tools to bin dir.

```
cp -r py/ path/to/llvm/build/bin
```



##### Test

- Change the current directory to test.

```
cd test
```

EH-Miner requires the domains and projects organized as follows. Also, the domains and projects should be specified in a config file: test.conf.

```
workingdir
|
|----domain1
|        |----project1
|        |----project2
|
|----domain2
|        |----project3
|        |----project4
```

- Compile the first test app

```
cd webserver
tar xvf lighttpd-1.4.45.tar
cd lighttpd-1.4.45
./configure
bear make
cd ../..
```

- Compile the second test app

```
cd ftpserver
tar xvf bftpd-4.4.tar
cd bftpd
./configure
bear make
cd ../..
```

- Merge the .json file (requiring jq), this step will generate compile_commands.json.

```
sudo apt install jq
find . -name 'compile_commands.json' | xargs jq 'flatten' -s > ./compile_commands.json
```

- Converting the source code to structured data, this step will generate table 'branch_call' in test.db.

```
find . -name *.c | xargs clang-ehminer -p . -find-branch-call -database-file=test.db -config-file=test.conf
```

- Normalization, this step will generate two tables in test.db: condition_equivalence and function_action.

```
sudo pip install pandas z3-solver
ehminer.py -d test.db
```

