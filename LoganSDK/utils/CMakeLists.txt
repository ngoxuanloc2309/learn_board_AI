# Layer U - utils: cqueue, sx_filter, sx_hpf, sx_vad. Depends on no other layer.
idf_component_register(SRCS "cqueue/cqueue.c"
                            "filter/sx_filter.c"
                            "filter/sx_hpf.c"
                            "vad/sx_vad.c"
                    INCLUDE_DIRS "cqueue" "filter" "vad")