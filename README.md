# Freestanding `snprintf` and `vsnprintf`

Included in this repository is a relatively simple implementation of 
`snprintf` and `vsnprintf` that I wrote in the course of an hour or two 
for use in a hobbyist microkernel. I've found myself writing this code 
or similar several times in the past, so decided to write a reasonably 
comprehensive implementation of `snprintf` that covers most of my use 
cases.

It does *not* include support for:
  - Floating point: FP shouldn't be used in a kernel, and printing 
    floating point isn't something I'm interesting in figuring out at 
    the moment.
  - `%n`: I've never needed it. It would be reasonably trivial (5 or 6 
    lines) to implement.
  - Wide characters: I don't plan on supporting anything other than 
    UTF-8. Again, this should be a short patch if you need it.
  - `*` precision, i.e. specifying precision in the next argument: I 
    haven't needed it in the kernel. If you need it, it's a short patch 
    away. 
  - POSIX extensions such as `%m$` and `*n$` style format specifiers, as 
    well as the `'` (thousands separator) format flag. These are avoided 
    purely for simplicity of the implementation.

The code has been written with extensibility in mind, so all but the 
POSIX extensions and floating point support would be easy to add if 
needed.

# License: ISC

> Copyright &copy; 2018, Joe Davis <me@jo.ie>
>
> Permission to use, copy, modify, and/or distribute this software for 
> any purpose with or without fee is hereby granted, provided that the 
> above copyright notice and this permission notice appear in all 
> copies.
> 
> THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL 
> WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
> WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE 
> AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL 
> DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR 
> PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER 
> TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR 
> PERFORMANCE OF THIS SOFTWARE.
