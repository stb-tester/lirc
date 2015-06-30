
function find_reps()
{

    awk ' 
    BEGIN                   { reps = -1; button = "__nil" 
                              first_reps = -1; first_button = "__nil" 
                              print_on_exit = 1
                            }
    first_button == "__nil" { first_button = $3; 
                              first_reps = 0
                            }
    $3 == first_button      { first_reps += 1}
    $3 != button            { button = $3 
                              if (reps != first_reps && reps != -1) {
                                  print "Button " button \
                                             " repeats " reps " times"
                                  print "But first button repeats " \
                                      first_reps " times"
                                  print_on_exit = 0
                                  exit(2)
                              }
                              reps = 0
                              print $0
                            }
    $3 == button            { reps += 1}
    END                     { if (print_on_exit)
                                  print "All keys repeated: " first_reps  \
                                      >/dev/stderr
                            }'

}
