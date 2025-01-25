file(GLOB component_srcs "*.c")

idf_component_register(
    SRCS "${component_srcs}"
    PRIV_REQUIRES driver freertos
    INCLUDE_DIRS "."
)
