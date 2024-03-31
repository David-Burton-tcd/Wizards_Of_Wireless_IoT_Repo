idf_component_register(SRCS "ultrasonic.c" "main.c"
                            "connectivity.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES ${project_dir}/certificates/cert.pem)