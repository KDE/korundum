project(KorundumModules)

macro_optional_find_package(Soprano)
macro_log_feature(SOPRANO_FOUND "Soprano" "Soprano libraries" "http://soprano.sourceforge.net" FALSE "" "Needed to compile Soprano bindings")

macro_optional_find_package(Nepomuk)
macro_log_feature(NEPOMUK_FOUND "Nepomuk" "Nepomuk libraries" "http://www.kde.org" FALSE "" "Needed to compile Nepomuk bindings")

macro_optional_find_package(KdepimLibs)
macro_log_feature(KDEPIMLIBS_FOUND "kdepimlibs" "KDE PIM libraries" "http://www.kde.org" FALSE "" "Needed to compile Akonadi bindings")

macro_optional_find_package(Akonadi)
macro_log_feature(AKONADI_FOUND "Akonadi" "Akonadi libraries" "http://www.kde.org" FALSE "" "Needed to compile Akonadi bindings")

macro_optional_find_package(Okular)
macro_log_feature(OKULAR_FOUND "Okular" "Okular libraries" "http://www.kde.org" FALSE "" "Needed to compile Okular bindings")

macro_optional_add_bindings(Soprano_FOUND "Soprano" soprano)

if(KDEPIMLIBS_FOUND AND Akonadi_FOUND)
    set(BUILD_Akonadi TRUE)
else(KDEPIMLIBS_FOUND AND Akonadi_FOUND)
    set(BUILD_Akonadi FALSE)
endif(KDEPIMLIBS_FOUND AND Akonadi_FOUND)
macro_optional_add_bindings(BUILD_Akonadi "Akonadi" akonadi)

add_subdirectory(kio)
add_subdirectory(khtml)
add_subdirectory(ktexteditor)
add_subdirectory(solid)
add_subdirectory(plasma)

# For nepomuk, we also must have found soprano, since soprano headers are used:
if(Soprano_FOUND AND Nepomuk_FOUND)
    set(BUILD_Nepomuk TRUE)
else(Soprano_FOUND AND Nepomuk_FOUND)
    set(BUILD_Nepomuk FALSE)
endif(Soprano_FOUND AND Nepomuk_FOUND)
macro_optional_add_bindings(BUILD_Nepomuk "Nepomuk" nepomuk)

macro_optional_add_bindings(OKULAR_FOUND "Okular" okular)

find_library(KATEINTERFACES_LIBRARY kateinterfaces HINTS ${KDE4_LIB_DIR})
find_path(KATE_INCLUDE_DIR kate/plugin.h HINTS ${KDE4_INCLUDE_DIR})

macro_optional_add_bindings(KATE_INCLUDE_DIR "Kate" kate)

macro_display_bindings_log()
