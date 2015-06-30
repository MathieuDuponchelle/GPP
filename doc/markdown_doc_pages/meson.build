m2m = find_program ('mallard_to_markdown.py')

markdown_pages = custom_target ('doc/markdown_doc_pages/GPP.Queue.markdown',
				output : 'GPP.Queue.markdown',
				command : [m2m, 'doc/c_doc_pages',
				'-o', 'doc/markdown_doc_pages',
				'-p', 'doc/python_doc_pages/',
				'-t', 'markdown'],
				depends : [c_doc_pages, python_doc_pages]
			       )
