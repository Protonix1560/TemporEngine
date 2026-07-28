
export LSAN_OPTIONS=suppressions=lsan-leaks.txt
# valgrind --tool=memcheck --leak-check=full --track-origins=yes --suppressions=valgrind.supp --num-callers=40 ./tempor --verbose=6
# valgrind --tool=helgrind ./tempor --verbose=6
./tempor --verbose=6
printf "\n\e[41m Returned with code $? \e[0m\n"
