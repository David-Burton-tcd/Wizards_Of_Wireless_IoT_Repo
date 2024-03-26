idf_component_register(SRCS "main.c"
                            "connectivity.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES ${project_dir}/certificates/cert.pem)
