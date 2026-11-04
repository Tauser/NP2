# H.264 Main/CABAC decoder for the camera stream (ADR-121).
idf_component_register(
    SRCS "np2_sub0h264.cpp"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "src" "include"
    PRIV_REQUIRES esp_timer heap
)

# The application builds at -Og. The measured figures in ADR-121 used -O2 for this
# header-only decoder. NDEBUG: upstream asserts on conforming-but-unsupported
# streams; the release path returns an error instead of aborting the panel.
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -DNDEBUG -Wno-error -Wno-unused-parameter)
