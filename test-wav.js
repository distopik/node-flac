const stream        = require('stream'),
	  	wav           = require('wav'),
	  	reader        = new wav.Reader(),
	  	{FlacEncoder} = require('./')

reader.on('format', fmt => {
	console.error(fmt)
	reader.pipe(new FlacEncoder(fmt)).pipe(process.stdout)
})

process.stdin.pipe(reader)