set(Folders2Copy "basis" "databases" "grids" "plugin" "python" "quadratures" "efpfrag")
INSTALL(DIRECTORY ${Folders2Copy} DESTINATION
${CMAKE_INSTALL_PREFIX}/share/psi)
