# PRISM

O Prism é a Interface de Leitura Independente de Plataforma para Fala e Mensagens (*Platform-agnostic Reader Interface for Speech and Messages*). Como o nome completo é um tanto longo, nós o chamamos apenas de **Prism**, para abreviar. O nome vem dos prismas da óptica — componentes ópticos transparentes com superfícies planas que refratam a luz em vários feixes. Daí a metáfora: refrate suas strings de TTS (Text-to-Speech) para enviá-las a diversos back-ends diferentes, potencialmente de forma simultânea.

O Prism visa unificar as várias bibliotecas de abstração de leitores de tela, como SpeechCore, UniversalSpeech, SRAL, Tolk, etc., em um único sistema unificado com uma única API. Naturalmente, também oferecemos suporte aos motores de TTS tradicionais. Tentei desenvolver o Prism de forma que a compilação seja trivial e não exija dependências externas; para esse fim, o configurador CMake baixará todas as dependências necessárias. No entanto, como ele utiliza o [cpm.cmake](https://github.com/cpm-cmake/CPM.cmake), a inclusão direta (*vendoring*) de dependências é perfeitamente possível.

---

## Compilação (Building)

Para compilar o Prism, tudo o que você precisa fazer é criar um diretório de build e executar o cmake como faria normalmente. As seguintes opções de compilação estão disponíveis:

| Opção | Descrição |
| --- | --- |
| `PRISM_ENABLE_TESTS` | Compila a suíte de testes (atualmente reservado). |
| `PRISM_ENABLE_DEMOS` | Habilita a compilação de aplicativos de demonstração para demonstrar o Prism em geral ou em linguagens específicas. |
| `PRISM_ENABLE_LINTING` | Habilita o linting do código-fonte com clang-tidy e outras ferramentas de análise estática. |
| `PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS` | **NÃO USE.** Ativa opções usadas principalmente pelo gerenciador de pacotes vcpkg. |

O Prism também está disponível no **vcpkg**. Para instalá-lo:

```bash
vcpkg install ethindp-prism

```

Os seguintes recursos (*features*) estão disponíveis:

| Recurso | Descrição |
| --- | --- |
| `speech-dispatcher` | Permite a ligação ao Speech Dispatcher e, por extensão, habilita seu módulo de back-end. Se não definido, o Speech Dispatcher NÃO será um back-end compatível. |
| `orca` | Permite o uso de glib e gdbus para comunicação direta com o leitor de tela Orca. Se não definido, o Orca NÃO estará disponível como back-end compatível. |

---

## Documentação

A documentação utiliza o [mdbook](https://github.com/rust-lang/mdBook). Para visualizá-la offline, instale o mdbook e execute `mdbook serve` dentro do diretório `doc`.

## API

A API está totalmente documentada na documentação mencionada acima. Se a documentação e os cabeçalhos (*headers*) não estiverem alinhados em termos de garantias ou expectativas, isso é considerado um bug e deve ser relatado.

## Bindings (Vínculos)

Atualmente, o desenvolvimento de bindings é um esforço contínuo. Existem os seguintes vínculos disponíveis:

| Linguagem | Pacote/Complemento |
| --- | --- |
| .NET | [prismatoid](https://www.nuget.org/packages/prismatoid) |
| Python | [Prismatoid](https://pypi.org/project/prismatoid) |

Novos bindings são bem-vindos. Se você escrever vínculos e desejar adicioná-los aqui, por favor, envie um Pull Request (PR)!

---

## Licença

Este projeto está licenciado sob a **Mozilla Public License versão 2.0**. Detalhes completos estão disponíveis no arquivo `LICENSE`.

Este projeto utiliza código de outros projetos. Especificamente:

* A ponte **SAPI** é creditada ao projeto [NVGT](https://github.com/samtupy/nvgt), bem como as funções `range_convert` e `range_convert_midpoint` em `utils.h` e `utils.cpp`. Atribuição semelhante vai para o NVGT pelo back-end do leitor de tela para Android.
* A biblioteca `simdutf` está licenciada sob a licença **Apache-2.0**.
* No Windows, o Prism inclui definições RPC do cliente controlador do **NVDA**, originalmente sob LGPL-2.1 (e stubs RPC gerados a partir dessas entradas). O projeto Prism recebeu permissão para licenciar os arquivos IDL (e seus resultados gerados) sob a **MPL-2.0**, independentemente da licença original. Portanto, você pode assumir que eles estão licenciados sob a MPL-2.0. Os cabeçalhos LGPL permanecem para fins de atribuição.

---

## Contribuindo

Contribuições são bem-vindas. Isso inclui, mas não se limita a: melhorias na documentação, novos back-ends, bindings, melhorias no sistema de build, etc. O projeto utiliza **C++23**, portanto, certifique-se de que seu compilador suporte esse padrão.
