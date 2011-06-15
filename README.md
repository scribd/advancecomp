# AdvanceCOMP

The [AdvanceCOMP][] recompression utilities is a suit of commands for optimizing the compression ratio for a number of [zlib][] based file formats, such as `.png`, `.mng`, and `.gz`.  

This is a fork of the [AdvanceCOMP][] recompression utilities that has been modified so that the `advpng` recompression tool can recompress Apples iPhone optimized `.png` proprietery format.

The files in the `src/` directory are the unpacked contents of the [`advancecomp-1.15.tar.gz`](http://sourceforge.net/projects/advancemame/files/advancecomp/1.15/advancecomp-1.15.tar.gz) distribution.

**See also:**<br />
&nbsp;&nbsp;&nbsp;&nbsp;[Xcode Build Setting Reference &ndash; COMPRESS_PNG_FILES (Compress .png files)](http://developer.apple.com/library/prerelease/ios/documentation/DeveloperTools/Reference/XcodeBuildSettingRef/1-Build_Setting_Reference/build_setting_ref.html#//apple_ref/doc/uid/TP40003931-CH3-SW6)<br />
&nbsp;&nbsp;&nbsp;&nbsp;[Technical Q&A QA1681 &ndash; Viewing iPhone-Optimized PNGs](http://developer.apple.com/library/prerelease/ios/#qa/qa1681/_index.html)<br />
&nbsp;&nbsp;&nbsp;&nbsp;[CgBI file format](http://iphonedevwiki.net/index.php/CgBI_file_format)

### Background

[zlib][] is a [LZ77][] based compression scheme, and like all [LZ77][] based compressors, it uses a <code><em>length</em>, <em>distance</em></code> pair in the compressed byte stream that is used by the decompressed to copy <code><em>length</em></code> number of bytes from a position that is a <code><em>distance</em></code> number of bytes back from the bytes that have already been decompressed.

In practice, there are usually a number of possible matches for any given <code><em>length</em>, <em>distance</em></code> pair.  Since the number of combinations raises very quickly, there is no practical way to find the combination that results in the smallest number of compressed bytes.  A number of heuristics are used by [LZ77][] based compressors, and the compression ratio achieved by a particular implementation is highly dependent on the <code><em>length</em>, <em>distance</em></code> search algorithm used.

The [AdvanceCOMP][] recompression tools use the search / match implementation from the [7-Zip][] / [LZMA][] compression engine which has been specifically designed to maximize the compression ratio.  The [7-Zip][] / [LZMA][] matcher is typically able to get an additional 4% to 15% compression than [zlib][] at its highest setting (i.e., `gzip -9`).  Of course, it also takes a lot longer to perform the compression.  Therefore it makes sense to spend the extra effort *only* when the files in question are of the "compress once, decompress many times" variety.

As a general rule, decompression speed of [LZ77][] based compressors is completely independent of the quality and implementation of the compressor.  In fact, it usually takes less time to decompress a highly compressed [LZ77][] byte stream (i.e., [AdvanceCOMP] recompressed files) than it does to decompress a [LZ77][] byte stream that was created by a compressor that was optimized for compression speed (i.e., `gzip -1`).  This is because a highly compressed [LZ77][] stream invariably contains less <code><em>length</em>, <em>distance</em></code> pairs that need to be processed, while the <code><em>length</em></code>, or the number of bytes copied, tends to be greater, and copying bytes is typically something that is highly optimized (i.e., `memcpy`).

[AdvanceCOMP]: http://advancemame.sourceforge.net/comp-readme.html
[zlib]: http://www.zlib.net/
[LZ77]: http://en.wikipedia.org/wiki/LZ77
[LZMA]: http://en.wikipedia.org/wiki/Lzma
[7-Zip]: http://en.wikipedia.org/wiki/7-Zip

