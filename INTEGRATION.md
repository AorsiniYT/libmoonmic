# libmoonmic - Integración con vita-moonlight

## Compilación

La biblioteca se compila automáticamente con vita-moonlight:

```bash
cd /mnt/i/vita/Proyecto/vita-moonlight
mkdir -p build && cd build

# Para PS Vita
cmake -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake ..
make

# Para Windows (MinGW)
cmake -DCMAKE_TOOLCHAIN_FILE=... ..
make

# Para Linux
cmake ..
make
```

## Dependencias

### PS Vita
- **Opus**: Incluido en VITASDK
- **SceAudio**: Incluido en VITASDK

### Windows
- **Opus**: Descargar de https://opus-codec.org/ o usar vcpkg
- **WASAPI**: Incluido en Windows SDK

### Linux
```bash
sudo apt-get install libopus-dev libpulse-dev
```

## Uso Básico

```cpp
#include "moonmic.h"

// Configuración
moonmic_config_t config = {
    .host_ip = "192.168.1.100",
    .port = 48100,
    .sample_rate = 48000,
    .channels = 1,
    .bitrate = 64000,
    .auto_start = true
};

// Crear y iniciar
moonmic_client_t* mic = moonmic_create(&config);
if (mic) {
    // Ya está transmitiendo (auto_start = true)
    
    // Detener cuando sea necesario
    moonmic_stop(mic);
    moonmic_destroy(mic);
}
```

## Integración con GameStreamClient

Ver `examples/integration_example.cpp` para un ejemplo completo.

Pasos básicos:

1. Incluir header: `#include "moonmic.h"`
2. Crear instancia al iniciar sesión
3. Destruir al cerrar sesión

## Callbacks

```cpp
void error_callback(const char* error, void* userdata) {
    printf("Error: %s\n", error);
}

void status_callback(bool connected, void* userdata) {
    printf("Status: %s\n", connected ? "Connected" : "Disconnected");
}

moonmic_set_error_callback(mic, error_callback, NULL);
moonmic_set_status_callback(mic, status_callback, NULL);
```

## Configuración del Host

En el host, debe estar ejecutándose `moonmic-host` (ver proyecto separado).

```bash
# Linux
moonmic-host --config /etc/moonmic.conf

# Windows
moonmic-host.exe --config moonmic.conf
```

## Solución de Problemas

### No se escucha audio

1. Verificar que `moonmic-host` está ejecutándose
2. Verificar firewall (puerto 48100 UDP)
3. Verificar IP del host en configuración

### Alta latencia

1. Reducir `buffer_size_ms` en configuración del host
2. Verificar red (ping al host)
3. Usar conexión cableada si es posible

### Errores de compilación

**PS Vita**: Asegurar que VITASDK está correctamente instalado
**Windows**: Verificar que Opus está en el PATH
**Linux**: Instalar `libopus-dev` y `libpulse-dev`

## API Completa

Ver `moonmic.h` para documentación completa de la API.

Funciones principales:
- `moonmic_create()` - Crear instancia
- `moonmic_destroy()` - Destruir instancia
- `moonmic_start()` - Iniciar transmisión
- `moonmic_stop()` - Detener transmisión
- `moonmic_is_active()` - Verificar estado
- `moonmic_set_error_callback()` - Callback de errores
- `moonmic_set_status_callback()` - Callback de estado
- `moonmic_version()` - Versión de la biblioteca
