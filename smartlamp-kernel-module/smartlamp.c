#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>

MODULE_AUTHOR("DevTITANS <devtitans@icomp.ufam.edu.br>");
MODULE_DESCRIPTION("Driver de acesso ao SmartLamp (ESP32 com Chip Serial CP2102");
MODULE_LICENSE("GPL");

#define MAX_RECV_LINE 100 // Tamanho máximo de uma linha de resposta do dispositvo USB



static char recv_line[MAX_RECV_LINE];              // Armazena dados vindos da USB até receber um caractere de nova linha '\n'
static struct usb_device *smartlamp_device;        // Referência para o dispositivo USB
static uint usb_in, usb_out;                       // Endereços das portas de entrada e saida da USB
static char *usb_in_buffer, *usb_out_buffer;       // Buffers de entrada e saída da USB
static char *cmd_buffer;                            // Buffer para montar o comando completo
static int usb_max_size;                           // Tamanho máximo de uma mensagem USB

#define VENDOR_ID   0x10c4 /* Encontre o VendorID  do smartlamp */
#define PRODUCT_ID  0xea60 /* Encontre o ProductID do smartlamp */
static const struct usb_device_id id_table[] = { { USB_DEVICE(VENDOR_ID, PRODUCT_ID) }, {} };

static int  usb_probe(struct usb_interface *ifce, const struct usb_device_id *id); // Executado quando o dispositivo é conectado na USB
static void usb_disconnect(struct usb_interface *ifce);                           // Executado quando o dispositivo USB é desconectado da USB
static int  usb_send_cmd(char *cmd, int param); 

// Executado quando o arquivo /sys/kernel/smartlamp/{led, ldr} é lido (e.g., cat /sys/kernel/smartlamp/led)
static ssize_t attr_show(struct kobject *sys_obj, struct kobj_attribute *attr, char *buff);
// Executado quando o arquivo /sys/kernel/smartlamp/{led, ldr} é escrito (e.g., echo "100" | sudo tee -a /sys/kernel/smartlamp/led)
static ssize_t attr_store(struct kobject *sys_obj, struct kobj_attribute *attr, const char *buff, size_t count);   
// Variáveis para criar os arquivos no /sys/kernel/smartlamp/{led, ldr}
static struct kobj_attribute  led_attribute = __ATTR(led, S_IRUGO | S_IWUSR, attr_show, attr_store);
static struct kobj_attribute  ldr_attribute = __ATTR(ldr, S_IRUGO | S_IWUSR, attr_show, attr_store);
static struct attribute      *attrs[]       = { &led_attribute.attr, &ldr_attribute.attr, NULL };
static struct attribute_group attr_group    = { .attrs = attrs };
static struct kobject        *sys_obj;                                             // Executado para ler a saida da porta serial

MODULE_DEVICE_TABLE(usb, id_table);

bool ignore = true;
int LDR_value = 0;

static struct usb_driver smartlamp_driver = {
    .name        = "smartlamp",     // Nome do driver
    .probe       = usb_probe,       // Executado quando o dispositivo é conectado na USB
    .disconnect  = usb_disconnect,  // Executado quando o dispositivo é desconectado na USB
    .id_table    = id_table,        // Tabela com o VendorID e ProductID do dispositivo
};

module_usb_driver(smartlamp_driver);

// Executado quando o dispositivo é conectado na USB
static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_endpoint_descriptor *usb_endpoint_in, *usb_endpoint_out;

    printk(KERN_INFO "SmartLamp: Dispositivo conectado ...\n");

    // Cria arquivos do /sys/kernel/smartlamp/*
    sys_obj = kobject_create_and_add("smartlamp", kernel_kobj);
    ignore = sysfs_create_group(sys_obj, &attr_group);

    // Detecta portas e aloca buffers de entrada e saída de dados na USB
    smartlamp_device = interface_to_usbdev(interface);
    ignore = usb_find_common_endpoints(interface->cur_altsetting, &usb_endpoint_in, &usb_endpoint_out, NULL, NULL);
    usb_max_size = usb_endpoint_maxp(usb_endpoint_in);
    usb_in = usb_endpoint_in->bEndpointAddress;
    usb_out = usb_endpoint_out->bEndpointAddress;
    usb_in_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    usb_out_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    cmd_buffer = kmalloc(MAX_RECV_LINE, GFP_KERNEL);
    // Testa a comunicação lendo o valor inicial do LDR
    LDR_value = usb_send_cmd("GET_LDR", 0);
    printk("SmartLamp: LDR Value inicial: %d\n", LDR_value);

    return 0;
}

// Executado quando o dispositivo USB é desconectado da USB
static void usb_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "SmartLamp: Dispositivo desconectado.\n");
    if (sys_obj) kobject_put(sys_obj);      // Remove os arquivos em /sys/kernel/smartlamp
    kfree(usb_in_buffer);                   // Desaloca buffers
    kfree(usb_out_buffer);
    kfree(cmd_buffer);                      // Desaloca buffers

}

// Envia um comando via USB, espera e retorna a resposta do dispositivo (convertido para int)
// Exemplo de Comando:  SET_LED 80
// Exemplo de Resposta: RES SET_LED 1
// Exemplo de chamada da função usb_send_cmd para SET_LED: usb_send_cmd("SET_LED", 80);
static int usb_send_cmd(char *cmd, int param) {
    int recv_size = 0;                      // Quantidade de caracteres no recv_line
    int ret, actual_size, i;
    int retries = 20;                       // Tenta algumas vezes receber uma resposta da USB. Depois desiste.
    char resp_expected[MAX_RECV_LINE];      // Resposta esperada do comando
    
    long resp_number = -1;                  // Número retornado pelo dispositivo

    // Limpa os buffers
    memset(recv_line, 0, MAX_RECV_LINE);
    memset(cmd_buffer, 0, MAX_RECV_LINE);
    memset(resp_expected, 0, MAX_RECV_LINE);

    // Monta o comando completo baseado no tipo
    if (strcmp(cmd, "SET_LED") == 0) {
        snprintf(cmd_buffer, MAX_RECV_LINE, "SET_LED %d\n", param);
        snprintf(resp_expected, MAX_RECV_LINE, "RES SET_LED");
    } else if (strcmp(cmd, "GET_LED") == 0) {
        snprintf(cmd_buffer, MAX_RECV_LINE, "GET_LED\n");
        snprintf(resp_expected, MAX_RECV_LINE, "RES GET_LED");
    } else if (strcmp(cmd, "GET_LDR") == 0) {
        snprintf(cmd_buffer, MAX_RECV_LINE, "GET_LDR\n");
        snprintf(resp_expected, MAX_RECV_LINE, "RES GET_LDR");
    } else {
        printk(KERN_ERR "SmartLamp: Comando desconhecido: %s\n", cmd);
        return -1;
    }

    printk(KERN_INFO "SmartLamp: Enviando comando: %s", cmd_buffer);

    // Envia o comando para a USB
    ret = usb_bulk_msg(smartlamp_device, usb_sndbulkpipe(smartlamp_device, usb_out), 
                       cmd_buffer, strlen(cmd_buffer), &actual_size, 1000);
    if (ret) {
        printk(KERN_ERR "SmartLamp: Erro de codigo %d ao enviar comando!\n", ret);
        return -1;
    }

    // Espera pela resposta correta do dispositivo
    while (retries > 0) {
        // Lê dados da USB
        ret = usb_bulk_msg(smartlamp_device, usb_rcvbulkpipe(smartlamp_device, usb_in), 
                          usb_in_buffer, min(usb_max_size, MAX_RECV_LINE), &actual_size, 1000);
        if (ret) {
            printk(KERN_ERR "SmartLamp: Erro ao ler dados da USB (tentativa %d). Codigo: %d\n", retries, ret);
            retries--;
            continue;
        }

        // Processa os dados recebidos, construindo linha por linha
        for (i = 0; i < actual_size; i++) {
            if (usb_in_buffer[i] == '\n' || recv_size >= MAX_RECV_LINE - 1) {
                recv_line[recv_size] = '\0'; // Termina a string
                
                printk(KERN_INFO "SmartLamp: Resposta recebida: %s\n", recv_line);
                
                // Verifica se a resposta é a esperada
                if (strncmp(recv_line, resp_expected, strlen(resp_expected)) == 0) {
                    // Extrai o número da resposta
                    if (strcmp(cmd, "SET_LED") == 0) {
                        // Para SET_LED, a resposta é "RES SET_LED 1" (sucesso) ou "RES SET_LED 0" (erro)
                        resp_number = simple_strtol(recv_line + strlen(resp_expected) + 1, NULL, 10);
                    } else {
                        // Para GET_LED e GET_LDR, a resposta é "RES GET_LED X" ou "RES GET_LDR Y"
                        resp_number = simple_strtol(recv_line + strlen(resp_expected) + 1, NULL, 10);
                    }
                    
                    printk(KERN_INFO "SmartLamp: Valor extraído: %ld\n", resp_number);
                    return (int)resp_number;
                }
                
                // Reinicia para a próxima linha
                recv_size = 0;
                memset(recv_line, 0, MAX_RECV_LINE);
            } else {
                recv_line[recv_size++] = usb_in_buffer[i];
            }
        }
        
        retries--;
    }

    printk(KERN_ERR "SmartLamp: Timeout - não recebeu resposta esperada\n");
    return -1; // Não recebeu a resposta esperada do dispositivo
}

// Executado quando o arquivo /sys/kernel/smartlamp/{led, ldr} é lido (e.g., cat /sys/kernel/smartlamp/led)
static ssize_t attr_show(struct kobject *sys_obj, struct kobj_attribute *attr, char *buff) {
    int value;
    const char *attr_name = attr->attr.name;

    printk(KERN_INFO "SmartLamp: Lendo %s ...\n", attr_name);

    // Lê o valor do led ou ldr baseado no nome do atributo
    if (strcmp(attr_name, "led") == 0) {
        value = usb_send_cmd("GET_LED", 0);
    } else if (strcmp(attr_name, "ldr") == 0) {
        value = usb_send_cmd("GET_LDR", 0);
    } else {
        printk(KERN_ERR "SmartLamp: Atributo desconhecido: %s\n", attr_name);
        return -EINVAL;
    }

    if (value < 0) {
        printk(KERN_ERR "SmartLamp: Erro ao ler %s\n", attr_name);
        return -EIO;
    }

    sprintf(buff, "%d\n", value);
    return strlen(buff);
}

// Executado quando o arquivo /sys/kernel/smartlamp/{led, ldr} é escrito (e.g., echo "100" | sudo tee -a /sys/kernel/smartlamp/led)
static ssize_t attr_store(struct kobject *sys_obj, struct kobj_attribute *attr, const char *buff, size_t count) {
    long ret, value;
    const char *attr_name = attr->attr.name;

    // Converte o valor recebido para long
    ret = kstrtol(buff, 10, &value);
    if (ret) {
        printk(KERN_ALERT "SmartLamp: valor de %s invalido.\n", attr_name);
        return -EACCES;
    }

    // Verifica se é um atributo válido para escrita
    if (strcmp(attr_name, "led") == 0) {
        // Valida o range do LED (0-100)
        if (value < 0 || value > 100) {
            printk(KERN_ALERT "SmartLamp: valor do LED deve estar entre 0 e 100.\n");
            return -EINVAL;
        }
        
        printk(KERN_INFO "SmartLamp: Setando %s para %ld ...\n", attr_name, value);
        
        // Envia o comando SET_LED com o valor
        ret = usb_send_cmd("SET_LED", (int)value);
        if (ret < 0) {
            printk(KERN_ALERT "SmartLamp: erro ao setar o valor do %s.\n", attr_name);
            return -EIO;
        }
    } else if (strcmp(attr_name, "ldr") == 0) {
        // LDR é somente leitura
        printk(KERN_ALERT "SmartLamp: LDR é somente leitura.\n");
        return -EACCES;
    } else {
        printk(KERN_ERR "SmartLamp: Atributo desconhecido: %s\n", attr_name);
        return -EINVAL;
    }

    return count;
}