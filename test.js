const crypto = require('crypto'),
	  fs     = require('fs'),
	  flac   = require('bindings')('node-flac'),
	  stream = new flac.FlacEncodeStream({numChannels: 2, depth: 24, streaming: true}),
	  buf    = new Buffer(256, true),
	  buf2   = new Buffer(256, true),
	  res    = []

for (let i = 0; i < 2560; i++) {
	crypto.randomBytes(buf.length).copy(buf)
	crypto.randomBytes(buf.length).copy(buf2)

	let b = stream.process([buf, buf2])
	if (b.length)
		res.push(b)
}

res.push(stream.finish())
fs.writeFileSync('output.flac', Buffer.concat(res))