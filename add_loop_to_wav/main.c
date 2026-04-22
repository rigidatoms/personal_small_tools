#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    char subchunk3ID[4];
    uint32_t subchunk3Size;
    uint32_t dwManufacturer;
    uint32_t dwProduct;
    uint32_t dwSamplePeriod;
    uint32_t dwMIDIUnityNote;
    uint32_t dwMIDIPitchFraction;
    uint32_t dwSMPTEFormat;
    uint32_t dwSMPTEOffset;
    uint32_t cSampleLoops;
    uint32_t cbSamplerData;
} smplHeader;

typedef struct {
    uint32_t dwIdentifier;
    uint32_t dwType;
    uint32_t dwStart;
    uint32_t dwEnd;
    uint32_t dwFraction;
    uint32_t dwPlayCount;
} smplLoop;
#pragma pack(pop)

void custom_fread(void*, size_t, size_t, FILE*); //Aplicando DRY.
int test_invalid_arg(char* buf, char* word_test, size_t size, FILE *f); //se retornar 0, significa que são iguais
int invalid_check_proc(char* msg, FILE *f); //Procedimento para checagem invalidada. Tratamento simplificado.
uint64_t buffer_to_uint(char* buffer, size_t size);
uint32_t skip_bytes(FILE* f);
void write_smpl_header(FILE*, uint32_t, uint32_t, uint32_t);

int main(int argc, char* argv[]){

    uint32_t loopStart = 0;

    switch (argc){
        case 1:
            printf("Uso: ./main.exe [arquivo .wav] {ponto de looping}\n");
            return 2;
        case 2:
            break;
        case 3:
            char *test;
            unsigned long val = strtoul(argv[2], &test, 10);
            if(val > 0xFFFFFFFF){
                printf("Valor fornecido é maior que o esperado para 32-bits.\n");
                return 2;
            }
            loopStart = (uint32_t)val;
            if(strcmp(test, "")) {
                printf("Ponto de loop inválido.\n");
                return 2;
            }
            break;
        default:
            printf("Quantidade de argumentos inválida. Uso: ./main.exe [arquivo .wav] {loop_point (uint32_t)}\n");
            return 2;      
    }

    char* wavPath = argv[1];
    char* suffix = ".wav";
    size_t lenStr = strlen(wavPath);
    if(strncmp(wavPath + lenStr - 4, suffix, 4)){
        printf("Não é um arquivo .wav\n"); // REMOVER
        return 2;
    }
    //O arquivo.
    FILE *wavFile = fopen(wavPath, "rb+");
    if(!wavFile){
        printf("Arquivo não pôde ser aberto.\n");
        exit(1);
    }
    //usando -Wall e -Wextra gera reclamações quando eu uso char* buffer (e deu problema de leitura mesmo)
    char buffer_16[2]; //buffer para 16 bits
    char buffer_32[4]; //buffer para 32 bits
    //conferir se tudo está correto
    if(test_invalid_arg(buffer_32, "RIFF", sizeof(uint32_t), wavFile))
    {
        return invalid_check_proc("Header (RIFF) inválido.", wavFile);
    }

    custom_fread(buffer_32, sizeof(uint32_t), 1, wavFile);
    long expected_end = (long)((uint32_t)buffer_to_uint(buffer_32, sizeof(uint32_t)) + 8u);
    fseek(wavFile, 0, SEEK_END);
    if(ftell(wavFile) != expected_end){
        return invalid_check_proc("Tamanho informado pelo header está incorreto.", wavFile);
    }
    fseek(wavFile, 8, SEEK_SET);
    
    if(test_invalid_arg(buffer_32, "WAVE", sizeof(uint32_t), wavFile))
    {
        return invalid_check_proc("Header (WAVE) inválido.", wavFile);
    }

    if(test_invalid_arg(buffer_32, "fmt ", sizeof(uint32_t), wavFile))
    {
        return invalid_check_proc("Header (fmt ) inválido.", wavFile);
    }
   
    fseek(wavFile, 4, SEEK_CUR); //pular cksize

    uint16_t wFormatTag = 0;
    custom_fread(buffer_16, sizeof(uint16_t), 1, wavFile);
    wFormatTag = (uint16_t)buffer_to_uint(buffer_16, sizeof(uint16_t));
    if(wFormatTag == 0xFFFE){
        return invalid_check_proc("WAVE_FORMAT_EXTENSIBLE não suportado.", wavFile);
    }
    
    fseek(wavFile, 2, SEEK_CUR); //pular nChannels

    uint32_t sample_rate = 0;
    custom_fread(buffer_32, sizeof(uint32_t), 1, wavFile);
    sample_rate = (uint32_t)buffer_to_uint(buffer_32, sizeof(sample_rate));
    
    fseek(wavFile, 4, SEEK_CUR); //pular nAvgBytesPerSec

    uint16_t nBlockAlign = 0;
    custom_fread(buffer_16, sizeof(uint16_t), 1, wavFile);
    nBlockAlign = (uint16_t)buffer_to_uint(buffer_16, sizeof(uint16_t));
    
    fseek(wavFile, 2, SEEK_CUR); //pular wBitsPerSample
    {
        uint16_t cbSize = 0;
        custom_fread(buffer_16, sizeof(uint16_t), 1, wavFile);
        cbSize = (uint16_t)buffer_to_uint(buffer_16, sizeof(cbSize));

        if(cbSize) fseek(wavFile, cbSize, SEEK_CUR); //pular extensão
    }
    
    uint32_t wavLen = 0;

    if(wFormatTag != 0x1){ 
        //então o formato em questão precisa do header "fact"
        if(test_invalid_arg(buffer_32, "fact", sizeof(uint32_t), wavFile))
        {
            return invalid_check_proc("Header (fact) inválido.", wavFile);
        }
        
        fseek(wavFile, 4, SEEK_CUR); //pular 4 bytes
        custom_fread(buffer_32, sizeof(uint32_t), 1, wavFile);
        wavLen = (uint32_t)buffer_to_uint(buffer_32, sizeof(wavLen)); //torna-se trivial atribuir dwSampleLength a wavLen
        /*
        De acordo com https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html: 
            *There is an ambiguity as to the meaning of "number of samples" for multichannel data. 
            *The implication in the Rev. 3 documentation is that it should be interpreted to be 
            *"number of samples per channel". The statement in the Rev. 3 documentation is:
            *
            *    The nSamplesPerSec field from the wave format header is used in conjunction 
            *    with the dwSampleLength field to determine the length of the data in seconds.
        */
        //então se algo der errado, a culpa é de quem escreveu a documentação.
    }
    
    {   //DATA pode não ser o próximo header, então garantir que seja
        int test_result = test_invalid_arg(buffer_32, "data", sizeof(uint32_t), wavFile);
        while(1)
        {
            uint32_t skip = skip_bytes(wavFile); 
            if(!test_result) { //achamos "data"
                if(wFormatTag == 0x1){
                    wavLen = skip/nBlockAlign;
                }
                fseek(wavFile, (skip % 2), SEEK_CUR); //pular padding
                break;
            }
            test_result = test_invalid_arg(buffer_32, "data", sizeof(uint32_t), wavFile);
        }
    } 
    
    if(wavLen == 0){ //Não obtivemos wavLen??????
        return invalid_check_proc("Erro ao obter o total de samples.", wavFile);
    }

    if(loopStart >= (wavLen - 1)){ //auto-explicativo - CONDIÇÃO NECESSÁRIA
        return invalid_check_proc("Ponto de loop inválido.", wavFile);
    }

    {
        //verificação importante!!! chegamos ao final do arquivo?
        long curr_pos = ftell(wavFile);
        
        if(curr_pos != expected_end){
            printf("Tem mais cabeçalhos no final do arquivo, verificando...\n");
            int test_result = test_invalid_arg(buffer_32, "smpl", sizeof(uint32_t), wavFile);
            while(test_result){
                skip_bytes(wavFile); //pular extensão
                if(ftell(wavFile) == expected_end) {
                    break;
                }
                test_result = test_invalid_arg(buffer_32, "smpl", sizeof(uint32_t), wavFile);
            }
            if(!test_result){ 
                //significa que já existe o header, então até que eu decida permitir modificar essa parte, vou deixar por conta do usuário
                printf("O header smpl já está presente no arquivo, por enquanto usar um hex editor para modificar os valores.\n");
                return 0;
            } 
            //do contrário, adicionar o header no fim do arquivo, como projetado
        }
    }
    
    //se chegamos até aqui, todas as checagens (feitas até agora) foram bem sucedidas
    write_smpl_header(wavFile, sample_rate, loopStart, wavLen);

    fclose(wavFile);

    return 0;
}

void custom_fread(void* buffer, size_t type_size, size_t quantity, FILE* f){
    if (fread(buffer, type_size, quantity, f) == 0){
        fprintf(stderr, "ERRO AO LER ARQUIVO.\n");
        fclose(f);
        exit(1);
    }
}

int test_invalid_arg(char* buf, char* word_test, size_t size, FILE *f){
    custom_fread(buf, size, 1, f);
    return strncmp(buf, word_test, size);
}

//Procedimento para checagem invalidada. Tratamento simples 
int invalid_check_proc(char* msg, FILE *f){
    printf("%s\n", msg);
    fclose(f);
    return 2;
}

uint64_t buffer_to_uint(char* buffer, size_t size){
    uint64_t result = 0;
    switch(size){
        case(sizeof(uint8_t)):
        case(sizeof(uint16_t)):
        case(sizeof(uint32_t)):
        case(sizeof(uint64_t)):
            //COMEÇAR CONVERSÃO
            uint64_t mask;
            int offset = 0;
            size_t i = 0;
            while(1){
                mask = 0xff << offset;
                result |= (((uint64_t)buffer[i] << offset) & mask);
                i += 1;
                if(i >= size) break;
                offset += 8;
            }
            //ENCERRAR CONVERSÃO
            break;
        default: //NÃO PERMITIR OUTROS TAMANHOS
            printf("Transformação inválida.\n");
    }
    return result;
}

uint32_t skip_bytes(FILE *f){
    char local_buffer[4];
    uint32_t bytes_to_skip;
    custom_fread(local_buffer, sizeof(uint32_t), 1, f);
    bytes_to_skip = (uint32_t)buffer_to_uint(local_buffer, sizeof(uint32_t));
    if(bytes_to_skip == 0) {
        fprintf(stderr, "ERRO AO PULAR BYTES.\n");
        fclose(f);
        exit(1);
    }
    fseek(f, bytes_to_skip, SEEK_CUR); //pular extensão
    return bytes_to_skip;
}

void write_smpl_header(FILE* f, uint32_t sample_rate, uint32_t loop_start, uint32_t wave_length){
    fseek(f, 0, SEEK_END);
    smplHeader smpl;
    memcpy(smpl.subchunk3ID, "smpl", 4);
    smpl.subchunk3Size = 36 + sizeof(smplLoop);
    smpl.dwManufacturer = smpl.dwProduct = smpl.dwMIDIPitchFraction =
        smpl.dwSMPTEFormat = smpl.dwSMPTEOffset = smpl.cbSamplerData = 0;
    smpl.dwSamplePeriod = (uint32_t)(1E9 / (double)sample_rate);
    smpl.dwMIDIUnityNote = 60;
    smpl.cSampleLoops = 1;
    fwrite(&smpl, sizeof(smpl), 1, f);

    smplLoop L;
    L.dwIdentifier = 0;
    L.dwType = 0;
    L.dwStart = loop_start;
    L.dwEnd = (wave_length)-1;
    L.dwFraction = 0;
    L.dwPlayCount = 0;
    fwrite(&L, sizeof(L), 1, f);

    uint32_t riff_chunkSize = ftell(f) - 8;
    fseek(f, 4, SEEK_SET);
    fwrite(&riff_chunkSize, sizeof(uint32_t), 1, f);
    printf("Operação bem sucedida.\n");
}