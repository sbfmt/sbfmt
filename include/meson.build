sbfmt_sources = ['sbfmt.c']

sbfmt_lib = library(
  'sbfmt',
  sbfmt_sources,
  include_directories: sbfmt_inc,
  version: meson.project_version(),
  soversion: 0,
  install: true,
)

install_headers(['../include/sbfmt.h'], subdir: 'sbfmt')

# Optionally build static lib
static_library('sbfmt_static', sbfmt_sources, include_directories: sbfmt_inc)
