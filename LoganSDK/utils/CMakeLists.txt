# Layer U - utils: cqueue, sx_filter. Depends on no other layer.
idf_component_register(SRCS "cqueue/cqueue.c"
                            "filter/sx_filter.c"
                    INCLUDE_DIRS "cqueue" "filter")