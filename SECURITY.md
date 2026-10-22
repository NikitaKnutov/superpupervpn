# Security Policy

If you find a security vulnerability in SuperPuperVPN, please **do not open a public issue**.

## Reporting a vulnerability

Report it privately through GitHub:
**Security → Advisories → "Report a vulnerability"** in this repository.

Please include:

- a description of the issue and its impact,
- steps to reproduce or a proof of concept,
- the affected version(s).

You can expect an acknowledgement within a few days. Once the issue is confirmed and a fix
is released, the advisory will be published with credit to the reporter (unless you prefer
to stay anonymous).

## Scope

SuperPuperVPN handles private keys and routes all traffic of the machine. The following are
especially in scope: leaks of the real IP or DNS, privilege escalation through the helper
service, profile decryption, supply-chain issues with the pinned core.

---

# Политика безопасности

Если вы нашли уязвимость в SuperPuperVPN, **не создавайте публичный issue**.

Сообщите о ней приватно через GitHub:
**Security → Advisories → «Report a vulnerability»** в этом репозитории.

Укажите описание проблемы, шаги для воспроизведения и затронутые версии. Ответ — в течение
нескольких дней. После исправления advisory будет опубликован с указанием автора находки
(или анонимно, по желанию).
