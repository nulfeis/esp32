#pragma once
#include <Arduino.h>

// Frases curadas a mano (no vienen de una API).
// La pantalla TFT no soporta tildes/ñ con las fuentes por defecto de
// TFT_eSPI, asi que el texto que se muestra va sin acentos. Version
// correcta en el comentario de cada una, para referencia.

struct Quote {
    const char *text;
    const char *author;
};

const Quote QUOTES_ES[] = {
    // "La vida es lo que pasa mientras estás ocupado haciendo otros planes."
    {"La vida es lo que pasa mientras estas ocupado haciendo otros planes.", "John Lennon"},
    // "El único modo de hacer un gran trabajo es amar lo que haces."
    {"El unico modo de hacer un gran trabajo es amar lo que haces.", "Steve Jobs"},
    // "Solo sé que no sé nada."
    {"Solo se que no se nada.", "Socrates"},
    // "La imaginación es más importante que el conocimiento."
    {"La imaginacion es mas importante que el conocimiento.", "Albert Einstein"},
    // "El éxito es ir de fracaso en fracaso sin perder el entusiasmo."
    {"El exito es ir de fracaso en fracaso sin perder el entusiasmo.", "Winston Churchill"},
    // "No es el más fuerte el que sobrevive, sino el que mejor se adapta al cambio."
    {"No es el mas fuerte el que sobrevive, sino el que mejor se adapta.", "Charles Darwin"},
    // "El conocimiento habla, pero la sabiduría escucha."
    {"El conocimiento habla, pero la sabiduria escucha.", "Jimi Hendrix"},
    // "El futuro pertenece a quienes creen en la belleza de sus sueños."
    {"El futuro pertenece a quienes creen en la belleza de sus suenos.", "Eleanor Roosevelt"},
    // "Todo lo que puedas imaginar es real."
    {"Todo lo que puedas imaginar es real.", "Pablo Picasso"},
    // "No cuentes los días, haz que los días cuenten."
    {"No cuentes los dias, haz que los dias cuenten.", "Muhammad Ali"},
    // "La perseverancia es la madre de la buena fortuna."
    {"La perseverancia es la madre de la buena fortuna.", "Miguel de Cervantes"},
    // "Nunca es tarde para ser lo que podrías haber sido."
    {"Nunca es tarde para ser lo que podrias haber sido.", "George Eliot"},
    // "La mejor forma de predecir el futuro es crearlo."
    {"La mejor forma de predecir el futuro es crearlo.", "Peter Drucker"},
    // "Cae siete veces, levántate ocho."
    {"Cae siete veces, levantate ocho.", "Proverbio japones"},
    // "Hay una fuerza más poderosa que el vapor o la electricidad: la voluntad."
    {"Hay una fuerza mas poderosa que el vapor o la electricidad: la voluntad.", "Albert Einstein"},
    // "Los límites de mi lenguaje son los límites de mi mundo."
    {"Los limites de mi lenguaje son los limites de mi mundo.", "Ludwig Wittgenstein"},
    // "La felicidad no es algo hecho. Proviene de tus propias acciones."
    {"La felicidad no es algo hecho. Proviene de tus propias acciones.", "Dalai Lama"},
    // "Quien tiene un porqué para vivir puede soportar casi cualquier cómo."
    {"Quien tiene un porque para vivir puede soportar casi cualquier como.", "Friedrich Nietzsche"},
};

const int QUOTES_ES_COUNT = sizeof(QUOTES_ES) / sizeof(QUOTES_ES[0]);
