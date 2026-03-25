//
//  Chip8.cpp
//  JustForPlay
//
//  Created by 胡新旸 on 2026/3/23.
//

#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include "Chip8.hpp"


    
    Chip8::Chip8(){
        memset(memory,  0, sizeof(memory));
        memset(V,       0, sizeof(V));
        memset(display, 0, sizeof(display));
        memset(keypad,  0, sizeof(keypad));
        memset(stack,   0, sizeof(stack));
        for (int i = 0; i < 80; i++)
            memory[0x50 + i] = fontset[i];
    }

    
    void Chip8::cycle(){
        if (pc >= 4094) {
                std::cerr << "PC out of bounds: " << std::hex << pc << std::endl;
                return;
            }
        //1.Fetch
        uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];
        pc += 2;
        //2.Decode & Execute
        uint16_t nnn = opcode & 0x0fff;
        uint8_t n = opcode & 0x000f;
        uint8_t x = (opcode & 0x0f00) >> 8;
        uint8_t y = (opcode & 0x00f0) >> 4;
        uint8_t kk = opcode & 0x00ff;
        uint8_t pixel;
        
        //3.Execute
        
        switch (opcode & 0xf000) {
            case 0x0000:
                if (opcode == 0x00e0) {
                    memset(display, 0, sizeof(display));
                    drawFlag = true;
                } else if (opcode == 0x00ee){
                    pc = stack[--sp];
                }
                break;
            case 0x1000:
                pc = nnn;
                break;
            case 0x2000:
                if (sp >= 16) {
                        std::cerr << "Stack overflow!" << std::endl;
                        return;
                    }
                sp++;
                stack[sp - 1] = pc;
                pc = nnn;
                break;
            case 0x3000:
                if (V[x] == kk) {
                    pc += 2;
                }
                break;
            case 0x4000:
                if (V[x] != kk) {
                    pc += 2;
                }
                break;
            case 0x5000:
                if ((opcode & 0x000F) == 0 && V[x] == V[y]) {
                    pc += 2;
                }
                break;
            case 0x6000:
                V[x] = kk;
                break;
            case 0x7000:
                V[x] += kk;
                break;
            case 0x8000:
                switch (n) {
                    case 0x0:
                        V[x] = V[y];
                        break;
                    case 0x1:
                        V[x] = V[x] | V[y];
                        break;
                    case 0x2:
                        V[x] = V[x] & V[y];
                        break;
                    case 0x3:
                        V[x] = V[x] xor V[y];
                        break;
                    case 0x4:
                    {
                        uint16_t sum = static_cast<uint16_t>(V[x]) + static_cast<uint16_t>(V[y]);
                        V[0xF] = (sum > 0xFF) ? 1 : 0;
                        V[x] = static_cast<uint8_t>(sum & 0xFF);
                        break;
                    }
                    case 0x5:
                        V[0xF] = (V[x] >= V[y]) ? 1 : 0;
                        V[x] = static_cast<uint8_t>(V[x] - V[y]);
                        break;
                    case 0x6:
                        V[0xf] = V[x] & 0x1;
                        V[x] >>= 1;
                        break;
                    case 0x7:
                        V[0xF] = (V[y] >= V[x]) ? 1 : 0;
                        V[x] = static_cast<uint8_t>(V[y] - V[x]);
                        break;
                    case 0xe:
                        V[0xf] = (V[x] & 0x80) >> 7;
                        V[x] <<= 1;
                        break;
                    default:
                        std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
                        break;
                }
                break;
            case 0x9000:
                if ((opcode & 0x000F) == 0 && V[x] != V[y]) {
                    pc+=2;
                }
                break;
            case 0xa000:
                I = nnn;
                break;
            case 0xb000:
                pc = nnn + V[0];
                break;
            case 0xc000:
                V[x] = (rand() % 256) & kk;
                break;
            case 0xd000:
                V[0xf] = 0;
                for (int row = 0; row < n; row++) {
                    pixel = memory[I + row];
                    for (int col = 0; col < 8; col++) {
                        if ((pixel & (0x80 >> col)) != 0) {
                            int screenX = (V[x] + col) % 64;
                            int screenY = (V[y] + row) % 32;
                            if (display[screenY * 64 + screenX] == 1) {
                                V[0xF] = 1;
                            }
                            display[screenY * 64 + screenX] ^= 1;
                        }
                    }
                }
                drawFlag = true;
                break;
            case 0xe000:
                switch (opcode & 0x00ff) {
                    case 0x009e:
                        if (keypad[V[(opcode & 0x0F00) >> 8] & 0x0F] != 0) {
                            pc += 2;
                        }
                        break;
                    case 0x00a1:
                        if (keypad[V[(opcode & 0x0F00) >> 8] & 0x0F] == 0) {
                            pc += 2;
                        }
                        break;
                }
                break;
                
            case 0xf000:
                            switch (opcode & 0x00ff) {
                                case 0x0007:
                                    V[x] = delayTimer;
                                    break;
                                case 0x0015:
                                    delayTimer = V[x];
                                    break;
                                case 0x0018:
                                    soundTimer = V[x];
                                    break;

                                case 0x000a: // 等待按键 (阻塞)
                                    keyPress = false; // 每次执行前重置标志位
                                    for (int i = 0; i < 16; i++) {
                                        if (keypad[i] != 0) {
                                            V[x] = i;
                                            keyPress = true;
                                            break;
                                        }
                                    }
                                    if (!keyPress) {
                                        pc -= 2;
                                        return;
                                    }
                                    break;

                                case 0x001e:
                                    I += V[x];
                                    break;

                                case 0x0029: // 帮你补上的：加载对应字符的字体地址
                                    I = 0x50 + V[x] * 5;
                                    break;

                                case 0x0033: // BCD 码转换
                                    memory[I]     = V[x] / 100;         // 百位
                                    memory[I + 1] = (V[x] / 10) % 10;   // 十位
                                    memory[I + 2] = V[x] % 10;          // 个位
                                    break;

                                case 0x0055: // 寄存器存入内存
                                    for (int i = 0; i <= x; i++) {
                                        memory[I + i] = V[i];
                                    }
                                    break;

                                case 0x0065: // 内存读入寄存器
                                    for (int i = 0; i <= x; i++) {
                                        V[i] = memory[I + i];
                                    }
                                    break;

                                default:
                                    std::cerr << "Unknown Fx opcode: " << std::hex << opcode << std::endl;
                                    break;
                            }
                            break;
            default:
                std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
                break;
        }
        
    };
    
    
    
    bool Chip8::loadROM(const std::string& filename) {
        // 1. 以二进制模式 (std::ios::binary) 打开文件，并直接定位到文件末尾 (std::ios::ate)
        std::ifstream file(filename, std::ios::binary | std::ios::ate);

        if (file.is_open()) {
            // 2. 获取文件大小并回到文件开头
            std::streampos size = file.tellg();
            if (size <= 0) {
                std::cerr << "Error: ROM file is empty: " << filename << std::endl;
                return false;
            }
            char* buffer = new char[size];
            file.seekg(0, std::ios::beg);

            // 3. 将文件内容读入临时缓冲区
            file.read(buffer, size);
            file.close();

            // 4. 将缓冲区内容加载到 Chip-8 内存中，从 0x200 开始
            // 注意：要检查 ROM 大小是否超过了剩余内存 (4096 - 512 = 3584)
            for (int i = 0; i < size && (0x200 + i) < 4096; ++i) {
                memory[0x200 + i] = (uint8_t)buffer[i];
            }

            delete[] buffer;
            std::cout << "ROM loaded successfully. Size: " << size << " bytes." << std::endl;
            return true;
        } else {
            std::cerr << "Error: Failed to open ROM file: " << filename << std::endl;
            return false;
        }
    };
