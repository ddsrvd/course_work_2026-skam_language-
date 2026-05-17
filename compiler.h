#pragma once

#include "object.h"
#include "vm.h"

// главная функция компилятора: принимает текст, возвращает готовую функцию (или
// nullptr при ошибке)
ObjFunction *compile(const char *source);

// функция для сборщика мусора, чтобы он не удалял функции во время их
// компиляции
void markCompilerRoots();
