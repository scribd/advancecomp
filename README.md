# AdvanceCOMP

The [AdvanceCOMP][] recompression utilities is a suit of commands for optimizing the compression ratio for a number of [zlib][] based file formats, such as `.png`, `.mng`, and `.gz`.  

This is a fork of the [AdvanceCOMP][] recompression utilities that has been modified by adding the `adviphone` recompression tool that can recompress Apples iPhone optimized [PNG][] proprietary format.

The files in the `src/` directory are the unpacked contents of the [`advancecomp-1.15.tar.gz`](http://sourceforge.net/projects/advancemame/files/advancecomp/1.15/advancecomp-1.15.tar.gz) distribution.

**See also:**<br />
&nbsp;&nbsp;&nbsp;&nbsp;[Xcode Build Setting Reference &ndash; COMPRESS_PNG_FILES (Compress .png files)][COMPRESS_PNG_FILES]<br />
&nbsp;&nbsp;&nbsp;&nbsp;[Technical Q&A QA1681 &ndash; Viewing iPhone-Optimized PNGs](http://developer.apple.com/library/prerelease/ios/#qa/qa1681/_index.html)<br />
&nbsp;&nbsp;&nbsp;&nbsp;[CgBI file format](http://iphonedevwiki.net/index.php/CgBI_file_format)

### Usage

This fork of the [AdvanceCOMP][] tools does not modify any of the original tools, but adds an additional tool&ndash; `adviphone`.

The `adviphone` tool has a couple of options, which you can view via `--help`, but there is really only one option that is used in practice: `-z4`.  This specifies that the original `.png` file should be recompressed using the highest compression / maximum effort setting.  For example:

<pre>
shell% adviphone -z4 <i>FILE</i>.png
      308008      283951  92% <i>FILE</i>.png
</pre>
      
The original `.png` file is replaced with the recompressed version.  The `.png` file is left unmodified if `adviphone` was unable to make the `.png` smaller.

You can also wild-card batch process `.png` files too:

<pre>
shell% adviphone -z4 Images/*.png
         937         908  96% Images/CornerReading@2x.png
      147935      136238  92% Images/Default.png
      523332      479938  91% Images/Default@2x.png
         127         127 100% Images/DottedLine.png (Bigger 133)
         148         148 100% Images/DottedLine@2x.png
</pre>

**Helpful Hint:** The [AdvanceCOMP][] distribution also includes the `advdef` tool.  This tool performs the same recompression optimization on `.gz` files, and it is used exactly the same way as `adviphone`&ndash; <code>advdef -z4 <i>FILE</i>.gz</code>.

### Design

The `adviphone` tool was created by taking the [`repng.cc`][repng.cc] and copying it to [`reiphone.cc`][reiphone.cc].  The PNG optimization code from [`repng.cc`][repng.cc] was then completely removed, and code that only recompresses a [PNG][]'s `IDAT` chunk was added.

The `adviphone` tool performs only a single type of optimization: recompression of the `IDAT` chunk in a `.png` file.  The contents of the decompressed `IDAT` chunk are not examined nor modified in anyway, it is simply recompressed using the [7z][] [RFC 1950][] / [zlib][] compression engine. The other [PNG][] chunks in the `.png` file are passed through unmodified.

#### RFC 1950 / zlib Streams

[RFC 1950][] / [zlib][] allows for two types of streams:

* Normal.  This stream type includes a header, trailer, and `Adler-32` checksum of the data.
* Raw.  This stream type does not include a header, trailer, or a `Adler-32` checksum.

The [PNG specification][PNG] mandates that the `IDAT` chunk be compressed using the [normal, non-raw](http://www.w3.org/TR/PNG/#10Compression) stream format.

The iPhone optimized [PNG][] format requires the `IDAT` chunk to be compressed using the ***raw*** stream format.

#### Recompressing `IDAT`

The `adviphone` tool works by recompressing the `IDAT` chunk using the [7z][] [RFC 1950][] / [zlib][] compression engine, which can usually achieve an additional 3% to 7% additional compression relative to [zlib][] / `gzip -9` maximum compression setting (it's sort of like `gzip` that goes to `gzip -11`).

The iPhone optimized [PNG][] format includes an additional, non-standard [PNG][] chunk type: `CgBI`.  The presence or absence of this chunk type is used to determine whether or not the `IDAT` chunk is a *raw* or *normal* [RFC 1950][] / [zlib][] stream.  The `adviphone` tool works on either iPhone optimized `.png` files, or [PNG] standard conforming `.png` files.  The presence or absence of a `CgBI` chunk is used to determine whether or not the `IDAT` chunk should be read and re-written as a *raw* or *normal* [RFC 1950][] / [zlib][] stream.

### Recompression Results

Some numbers (obtained with `shell% wc Images/*.png | tail -1`) based on the `.png` files in `Resources/Images` at the time of this writing:

<table>
<tr><td align="left">Default Xcode <code>pngcrush</code></td><td align="left">6554837 bytes (6.25MB)</td></tr>
<tr><td align="left">After <code>adviphone</code></td><td align="left">6110164 bytes (5.82MB)</td></tr>
</table>

This represents a savings of 444673 bytes, or 434KB.

There is an additional option that you can use with `pngcrush`: `-brute`.  This option tries a very large number of permutations of the various compression knobs, which means it can take a ***lot*** longer.  Here's the results with `-brute` enabled:

<table>
<tr><td align="left">Xcode <code>pngcrush</code> w/ <code>-brute</code></td><td align="left">5626755 bytes (5.36MB)</td></tr>
<tr><td align="left">After <code>adviphone<code></td><td align="left">5248702 bytes (5.00MB)</td></tr>
</table>

Using a combination of both `-brute` and `adviphone` saves 1306135 bytes, or 1.24MB, relative to the default Xcode [`COMPRESS_PNG_FILES`][COMPRESS_PNG_FILES] optimization.

### Background / Theory

[zlib][] is a [LZ77][] based compression scheme, and like all [LZ77][] based compressors, it uses a <code><em>length</em>, <em>distance</em></code> pair in the compressed byte stream that is used by the decompressed to copy <code><em>length</em></code> number of bytes from a position that is a <code><em>distance</em></code> number of bytes back from the bytes that have already been decompressed.

In practice, there are usually a number of possible matches for any given <code><em>length</em>, <em>distance</em></code> pair.  Since the number of combinations raises very quickly, there is no practical way to find the combination that results in the smallest number of compressed bytes.  A number of heuristics are used by [LZ77][] based compressors, and the compression ratio achieved by a particular implementation is highly dependent on the <code><em>length</em>, <em>distance</em></code> search algorithm used.

The [AdvanceCOMP][] recompression tools use the search / match implementation from the [7-Zip][] / [LZMA][] compression engine which has been specifically designed to maximize the compression ratio.  The [7-Zip][] / [LZMA][] matcher is typically able to get an additional 4% to 15% compression than [zlib][] at its highest setting (i.e., `gzip -9`).  Of course, it also takes a lot longer to perform the compression.  Therefore it makes sense to spend the extra effort *only* when the files in question are of the "compress once, decompress many times" variety.

As a general rule, decompression speed of [LZ77][] based compressors is completely independent of the quality and implementation of the compressor.  In fact, it usually takes less time to decompress a highly compressed [LZ77][] byte stream (i.e., [AdvanceCOMP] recompressed files) than it does to decompress a [LZ77][] byte stream that was created by a compressor that was optimized for compression speed (i.e., `gzip -1`).  This is because a highly compressed [LZ77][] stream invariably contains less <code><em>length</em>, <em>distance</em></code> pairs that need to be processed, while the <code><em>length</em></code>, or the number of bytes copied, tends to be greater, and copying bytes is typically something that is highly optimized (i.e., `memcpy`).

[AdvanceCOMP]: http://advancemame.sourceforge.net/comp-readme.html
[zlib]: http://www.zlib.net/
[LZ77]: http://en.wikipedia.org/wiki/LZ77
[LZMA]: http://en.wikipedia.org/wiki/Lzma
[7-Zip]: http://en.wikipedia.org/wiki/7-Zip
[repng.cc]: https://github.com/scribd/advancecomp/blob/master/src/repng.cc
[reiphone.cc]: https://github.com/scribd/advancecomp/blob/master/src/reiphone.cc
[RFC 1950]: http://www.ietf.org/rfc/rfc1950.txt
[PNG]: http://www.w3.org/TR/PNG/
[7z]: http://en.wikipedia.org/wiki/7z
[COMPRESS_PNG_FILES]: http://developer.apple.com/library/prerelease/ios/documentation/DeveloperTools/Reference/XcodeBuildSettingRef/1-Build_Setting_Reference/build_setting_ref.html#//apple_ref/doc/uid/TP40003931-CH3-SW6
