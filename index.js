const stream = require('stream'),
      flac   = require('bindings')('node-flac')

/* export */ class FlacEncoder extends stream.Transform {
	constructor(format) {
		super(format)
		this.format  = format
		if (!this.format.blockAlign) {
			this.format.blockAlign = this.format.bitDepth * this.format.channels / 8
		}
		this.remainder = null
		this.encoder   = new flac.FlacEncodeStream(format)
	}

	_transform(chunk, encoding, callback) {
		if (this.remainder) {
			chunk = Buffer.concat([this.remainder, chunk], this.remainder.length + chunk.length)
			this.remainder = null
		}
		const tail = chunk.length % this.format.blockAlign
		if (tail) {
			this.remainder = chunk.slice(-tail)
			chunk          = chunk.slice(0, chunk.length - tail)
		}
		/* TODO: there are probably other options than interleaved */
		const buf = this.encoder.processInterleaved(chunk)
		if (buf.length) 
			this.push(buf)
		callback()
	}

	_flush(callback) {
		if (this.remainder) {
			const buf = this.encoder.processInterleaved(this.remainder)
			if (buf.length) 
				this.push(buf)
		}
		const buf = this.encoder.finish()
		if (buf.length)
			this.push(buf)
		callback()	
	}
}

module.exports = {
	FlacEncoder,
	flac
}