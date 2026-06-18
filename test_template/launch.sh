
export LSAN_OPTIONS=suppressions=lsan-leaks.txt
# valgrind --tool=memcheck --leak-check=full --track-origins=yes --suppressions=valgrind.supp ./tempor --verbose=6
./tempor --verbose=6
