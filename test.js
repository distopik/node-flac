const crypto = require('crypto'),
	  fs     = require('fs'),
	  flac   = require('bindings')('node-flac'),
	  stream = new flac.FlacEncodeStream({numChannels: 1, streaming: true}),
	  buf    = new Buffer(256, true),
	  res    = []

for (let i = 0; i < 256; i++) {
	crypto.randomBytes(buf.length).copy(buf)
	let b = stream.process([buf])
	if (b.length)
		res.push(b)
}

res.push(stream.finish())
fs.writeFileSync('output.flac', Buffer.concat(res))