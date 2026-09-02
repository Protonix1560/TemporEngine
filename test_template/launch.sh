
export LSAN_OPTIONS=suppressions=lsan-leaks.txt
export TSAN_OPTIONS=second_deadlock_stack=1
export VK_LAYER_DUPLICATE_MESSAGE_LIMIT=0

# gdb --args ./tempor --verbose=6
# valgrind --tool=helgrind --vgdb=full --vgdb-error=0 ./tempor --verbose=6

# valgrind --tool=memcheck --leak-check=full --track-origins=yes --suppressions=valgrind.supp --num-callers=40 ./tempor --verbose=6
# valgrind --tool=helgrind ./tempor --verbose=6

./tempor --verbose=6

printf "\n\e[41m Returned with code $? \e[0m\n"
