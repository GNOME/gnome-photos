xsltproc_command = [
  xsltproc,
  '--nonet',
  '--output', '@OUTPUT@',
  '--stringparam', 'funcsynopsis.style', 'ansi',
  '--stringparam', 'man.authors.section.enabled', '0',
  '--stringparam', 'man.copyright.section.enabled', '0',
  '--stringparam', 'man.output.quietly', '1',
  'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
  '@INPUT@',
]

manual = 'gnome-photos'

input = manual + '.xml'
output = manual + '.1'

custom_target(
  output,
  command: xsltproc_command,
  input: input,
  install: true,
  install_dir: join_paths(photos_mandir, 'man1'),
  output: output,
)
