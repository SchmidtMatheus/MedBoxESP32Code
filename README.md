# Envio de Dados de Monitoramento via Wi-Fi

Este é um exemplo de código para um dispositivo ESP32 que monitora a temperatura e umidade ambiente e envia esses dados para um servidor via Wi-Fi.

## Pré-requisitos

- Placa ESP32 (como o ESP32 Dev Kit)
- Sensor de temperatura e umidade DHT11 (ou outro sensor compatível)
- Conexão Wi-Fi disponível
- Servidor para receber os dados (pode ser um servidor local ou na nuvem)

## Instalação

1. Clone ou faça o download deste repositório.
2. Abra o arquivo `codigo.cpp` em sua IDE Arduino.

## Configuração

Antes de compilar e carregar o código para o ESP32, certifique-se de fazer as seguintes configurações:

- Defina o SSID e a senha do seu Wi-Fi na função `setup()`.

    ```cpp
    WiFi.begin("SEU_SSID", "SUA_SENHA");
    ```

- Substitua o endpoint do servidor na função `processarFila()` para o seu próprio.

    ```cpp
    http.begin("http://seuservidor.com/endpoint");
    ```

## Uso

1. Conecte o ESP32 ao computador.
2. Compile e carregue o código para o ESP32.
3. O ESP32 começará a monitorar a temperatura e umidade ambiente.
4. Quando houver conexão Wi-Fi, ele enviará os dados para o servidor.

## Como Funciona

- O ESP32 lê os dados de temperatura e umidade do sensor DHT11 a cada intervalo definido.
- Os dados são armazenados em uma fila para posterior envio via Wi-Fi.
- Quando há conexão Wi-Fi, os dados são enviados para o servidor.
- Os dados enviados com sucesso são removidos da fila.
