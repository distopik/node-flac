# node-flac
Native libFLAC bindings for Node. Similar `format` object used as `node-wav` and `speaker`.

## Encoding:

```js
const flac      = require('node-flac'),
      wav       = require('node-wav'),
      fs        = require('fs'),
      wavReader = new wav.Reader()

wavReader.on('format', function (format) {
  const flacEncoder = new flac.FlacEncoder(format)

  wavReader.pipe(flacEncoder).pipe(fs.createWriteStream('output.flac'))
})

fs.createReadStream('track01.wav').pipe(wavReader)
```