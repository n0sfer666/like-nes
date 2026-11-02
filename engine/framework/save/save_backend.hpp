#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace save {

enum class ReadResult { Ok, Missing, TooLarge, Error };
enum class MoveResult { Moved, Exists, Error };

class Backend {
public:
    virtual ~Backend() = default;
    // Изменения одного ключа сериализует вызывающий; read/write не являются CAS между писателями.
    // TooLarge сохраняет первые limit байт: счётчик заголовка читается без загрузки тела.
    // Missing/Error очищают out. Вызывающий обязан явно задать предел.
    virtual ReadResult read(const std::string& key, std::vector<uint8_t>& out, size_t limit) = 0;
    virtual bool write(const std::string& key, std::span<const uint8_t> bytes) = 0;
    virtual bool list(std::vector<std::string>& keys) = 0;
    // Не перезаписывает to; Error сохраняет from. После Moved from отсутствует, to долговечен.
    virtual MoveResult move_new(const std::string& from, const std::string& to) = 0;
};

// Ключ — имя одного файла: a-z,0-9,._-, до128 байт, без DOS-имён/конечной точки;
// .tmp зарезервирован writer. Uppercase запрещён, чтобы case-insensitive FS не создавала алиасы.
// list возвращает только такие ключи. Корень только абсолютный; при отказе
// его создания/проверки операции возвращают ошибку, а не обращаются к текущему каталогу.
class LocalBackend final : public Backend {
public:
    explicit LocalBackend(std::string root);
    ReadResult read(const std::string& key, std::vector<uint8_t>& out, size_t limit) override;
    bool write(const std::string& key, std::span<const uint8_t> bytes) override;
    bool list(std::vector<std::string>& keys) override;
    MoveResult move_new(const std::string& from, const std::string& to) override;
    const std::string& root() const { return root_; }

private:
    std::string path(const std::string& key) const;
    std::string root_;
};

enum class SelectionResult { Selected, Empty, Corrupt, Conflict };
struct Selection {
    SelectionResult result = SelectionResult::Empty;
    size_t index = static_cast<size_t>(-1);
    uint64_t writes = 0;
};

// Проверяет контейнеры, выбирает наибольший counter, часы не участвуют. На равенстве
// разных валидных образов — Conflict без победителя; входные копии остаются неизменными.
Selection select_record(std::span<const std::vector<uint8_t>> copies);

} // namespace save
