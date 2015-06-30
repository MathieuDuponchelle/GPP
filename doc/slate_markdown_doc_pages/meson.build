m2m = find_program ('mallard_to_markdown.py')

slate_markdown_pages = custom_target ('doc/slate_markdown_doc_pages/GPP.Queue.markdown',
				output : 'GPP.Queue.markdown',
				command : [m2m, 'doc/c_doc_pages',
				'-o', 'doc/slate_markdown_doc_pages',
				'-p', 'doc/python_doc_pages/',
				'-i', 'doc/index.markdown',
				'-t', 'slate'],
				depends : [c_doc_pages, python_doc_pages]
			       )
