subdir('acl')
subdir('adouble')
subdir('bstring')
subdir('cnid')
subdir('compat')
subdir('dsi')
subdir('iniparser')
subdir('util')
subdir('unicode')
subdir('vfs')

libatalk_deps = []
libatalk_libs = []
libatalk_sources = []

if have_acls
    libatalk_deps += acl_deps
endif

if have_threads
    libatalk_deps += threads
endif

if have_appletalk
    subdir('asp')
    subdir('atp')
    subdir('nbp')
    subdir('netddp')
    libatalk_libs += [
        libasp,
        libatp,
        libnbp,
        libnetddp,
    ]
endif

if have_spotlight
    subdir('talloc')
    libatalk_libs += libtalloc
endif

libatalk = library(
    'atalk',
    libatalk_sources,
    include_directories: root_includes,
    dependencies: [libatalk_deps, libcnid_deps],
    link_whole: [
        libacl,
        libadouble,
        libatalk_libs,
        libbstring,
        libcnid,
        libcompat,
        libdsi,
        libiniparser,
        libunicode,
        libutil,
        libvfs,
    ],
    version: '19.0.0',
    soversion: '19',
    install: true,
)
