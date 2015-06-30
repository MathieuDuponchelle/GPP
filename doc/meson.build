sections_file = '@0@/@1@'.format(meson.current_source_dir(), 'gpp-sections.txt')

examples = '@0@/@1@'.format(meson.current_source_dir(), '../examples')
example_doc = '@0@/@1@'.format(meson.current_source_dir(), 'example_docs')
handwritten_doc = '@0@/@1@'.format(meson.current_source_dir(), 'handwritten')

c_doc_pages = gnome.generate_doc_from_gir(girtargets,
				       	  output_dir : 'c_doc_pages',
  				          dependencies: [],
					  link_to_gtk_doc: true,
					  online_links : true,
					  sections_file : sections_file,
					  markdown_includes : [handwritten_doc, example_doc, examples],
					  language : 'C'
				         )

python_doc_pages = gnome.generate_doc_from_gir(girtargets,
				       	       output_dir : 'python_doc_pages',
  				               dependencies: [],
					       link_to_gtk_doc: true,
					       online_links : true,
					       sections_file : sections_file,
					       markdown_includes : [handwritten_doc, example_doc, examples],
					       language : 'python'
				              )

subdir('markdown_doc_pages')
subdir('slate_markdown_doc_pages')

sh = find_program ('sh')
update_slate = find_program ('update_slate.sh')
slate_dir = '@0@/@1@'.format(meson.current_source_dir(), 'slate')
built_slate_dir = '@0@/@1@'.format(meson.current_build_dir(), 'slate')
markdown_pages = '@0@/@1@'.format(meson.current_build_dir(), 'slate_markdown_doc_pages')
index_markdown = '@0@/@1@'.format(meson.current_build_dir(), 'index.markdown')

slate = custom_target ('slate.stamp',
		       output : 'slate.stamp',
		       command : [sh, update_slate, built_slate_dir,
		       		  slate_dir, markdown_pages, index_markdown],
		       depends : slate_markdown_pages,
		      )
