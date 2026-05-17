#pragma once // Заменяем #ifndef на современную защиту

#include <cstddef> // Плюсовые аналоги сишных библиотек [cite: 801]
#include <cstdint>
#include <limits> // Для получения точных пределов типов

// #define DEBUG_PRINT_CODE      // вывод байткода после компиляции
// #define DEBUG_TRACE_EXECUTION // вывод стека при выполнении
//  #define DEBUG_STRESS_GC       // запуск GC при каждом выделении памяти
// #define DEBUG_LOG_GC // подробные логи работы GC в консоль

constexpr int UINT8_COUNT = 256;
