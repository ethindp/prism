# Introduction

Prism is the Platform-Agnostic Reader Interface for Speech and Messages. The actual expansion is a mouthful, so we just call it Prism.

Prism aims to take all the TTS and screen reader engines out there and to combine them into a single, unified library, with a single, easy to understand interface. Prism does this by being comprised of back-ends. The front-end is the C library, documented in the next section of this reference manual; back-ends are the actual text-to-speech or screen reader communication interfaces themselves. Back-ends may choose what to implement as they see fit; almost nothing is required.

## Conventions

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "NOT RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in [BCP 14](https://www.rfc-editor.org/bcp/bcp14) [[RFC2119](https://www.rfc-editor.org/rfc/rfc2119)] [[RFC8174](https://www.rfc-editor.org/rfc/rfc8174)] when, and only when, they appear in all capitals, as shown here.
