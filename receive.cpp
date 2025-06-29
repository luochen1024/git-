#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)
using namespace std;

const int MAXSIZE = 1024; // ���仺������󳤶�

const unsigned char SYN = 0x1;
// 001���� FIN = 0 ACK = 0 SYN = 1

const unsigned char ACK = 0x2;
// 010���� FIN = 0 ACK = 1 SYN = 0

const unsigned char ACK_SYN = 0x3;
// 011���� FIN = 0 ACK = 1 SYN = 1

const unsigned char FIN = 0x4;
// 100���� FIN = 1 ACK = 0 SYN = 0

const unsigned char FIN_ACK = 0x5;
// 101���� FIN = 1 ACK = 0 SYN = 1

const unsigned char OVER = 0x7;
// ������־ 111���� FIN = 1 ACK = 1 SYN = 1

double MAX_TIME = 0.5 * CLOCKS_PER_SEC;

u_short checkSum(u_short* mes, int size)
{
    int count = (size + 1) / 2;
    // buffer�൱��һ��Ԫ��Ϊu_short���͵����飬ÿ��Ԫ��16λ���൱����У��͹����е�һ��Ԫ��
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size); // ��message����buf
    u_long sum = 0;
    while (count--)
    {
        sum += *buf++;
        // ����н�λ�򽫽�λ�ӵ����λ
        if (sum & 0xffff0000)
        {
            sum &= 0xffff;
            sum++;
        }
    }
    // ȡ��
    return ~(sum & 0xffff);
}

struct HEADER
{
    u_short sum = 0;      // У��� 16λ
    u_short datasize = 0; // ���������ݳ��� 16λ
    unsigned char flag = 0;
    // ��λ��ʹ�ú���λ��������FIN ACK SYN
    unsigned char SEQ = 0;
    // ��λ����������кţ�0~255��������mod
    HEADER()
    {
        sum = 0;      // У���    16λ
        datasize = 0; // ���������ݳ���     16λ
        flag = 0;     // 8λ��ʹ�ú���λ��������FIN ACK SYN
        SEQ = 0;      // 8λ
    }
};

int Connect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    // ���յ�һ��������Ϣ
    while (1)
    {
        // ͨ���󶨵�socket���ݡ���������
        if (recvfrom(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) == -1)
        {
            return -1;
        }
        memcpy(&header, Buffer, sizeof(header));
        if (header.flag == SYN && checkSum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << " ���յ���һ���������� " << endl;
            break;
        }
    }
    // ���͵ڶ���������Ϣ
    header.flag = ACK;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));

    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        return -1;
    }
    else
    {
        cout << " �ɹ����͵ڶ����������� " << endl;
    }
    clock_t start = clock(); // ��¼�ڶ������ַ���ʱ��

    // ���յ���������
    while (recvfrom(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0)
    {
        // ��ʱ�ش�
        if (clock() - start > MAX_TIME)
        {
            cout << " �ڶ������ֳ�ʱ " << endl;
            header.flag = ACK;
            header.sum = 0;
            header.flag = checkSum((u_short*)&header, sizeof(header));
            memcpy(Buffer, &header, sizeof(header));
            if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
            {
                return -1;
            }
            cout << " �Ѿ��ش� " << endl;
        }
    }

    // �����յ��ĵ��������ֵ����ݰ�
    HEADER temp1;
    memcpy(&temp1, Buffer, sizeof(header));

    if (temp1.flag == ACK_SYN && checkSum((u_short*)&temp1, sizeof(temp1)) == 0)
    {
        cout << " ���յ���������������" << endl;
        cout << " �ɹ�����" << endl;
    }
    else
    {
        cout << " �������ݣ�������" << endl;
    }
    return 1;
}

int RecvMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char* message)
{
    long int fileLength = 0; // �ļ�����
    HEADER header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;

    while (1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen); // ���ձ��ĳ���
        // cout << length << endl;
        memcpy(&header, Buffer, sizeof(header));
        // �ж��Ƿ��ǽ���
        if (header.flag == OVER && checkSum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << " �ļ��������" << endl;
            break;
        }
        if (header.flag == static_cast<unsigned char>(0) && checkSum((u_short*)Buffer, length - sizeof(header)))
        {
            // ����յ������кų��������·���ACK����ʱseqû�б仯
            if (checkSum((u_short*)Buffer, length - sizeof(header)) == 0 && seq != int(header.SEQ))
            {
                // ˵���������⣬����ACK
                header.flag = ACK;
                header.datasize = 0;
                header.SEQ = (unsigned char)seq;
                header.sum = checkSum((u_short*)&header, sizeof(header));
                memcpy(Buffer, &header, sizeof(header));
                // �ط��ð���ACK
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                cout << " �ط���һ���ظ��� - ACK:" << (int)header.SEQ << " SEQ:" << (int)header.SEQ << endl;
                continue; // ���������ݰ�
            }
            seq = int(header.SEQ);
            if (seq > 255)
            {
                seq = seq - 256;
            }

            cout << " �յ��� " << length - sizeof(header) << " �ֽ� - Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
            char* temp = new char[length - sizeof(header)];
            memcpy(temp, Buffer + sizeof(header), length - sizeof(header));
            // cout << "size" << sizeof(message) << endl;
            memcpy(message + fileLength, temp, length - sizeof(header));
            fileLength = fileLength + int(header.datasize);

            // ����ACK
            header.flag = ACK;
            header.datasize = 0;
            header.SEQ = (unsigned char)seq;
            header.sum = 0;
            header.sum = checkSum((u_short*)&header, sizeof(header));
            memcpy(Buffer, &header, sizeof(header));
            // ���ؿͻ���ACK������ɹ��յ���
            sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            cout << " �ظ��ͻ��� - flag:" << (int)header.flag << " SEQ:" << (int)header.SEQ << " SUM:" << int(header.sum) << endl;
            seq++;
            if (seq > 255)
            {
                seq = seq - 256;
            }
        }
    }
    // ����OVER��Ϣ
    header.flag = OVER;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        return -1;
    }
    return fileLength;
}

int disConnect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    while (1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen); // ���ձ��ĳ���
        memcpy(&header, Buffer, sizeof(header));
        if (header.flag == FIN && checkSum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << " ���յ���һ�λ������� " << endl;
            break;
        }
        else
        {
            return -1;
        }
    }
    // ���͵ڶ��λ�����Ϣ
    header.flag = ACK;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        cout << " ���͵ڶ��λ���ʧ��" << endl;
        return -1;
    }
    else
    {
        cout << " �ɹ����͵ڶ��λ�������" << endl;
    }

    //
    header.flag = FIN_ACK;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header)); // ����У���
    if (sendto(sockServ, (char*)&header, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        return -1;
    }
    else
    {
        cout << " �ɹ����͵����λ�������" << endl;
    }
    clock_t start = clock();
    // ���յ��Ĵλ���
    while (recvfrom(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0)
    {
        if (clock() - start > MAX_TIME) // ��ʱ�����´�������λ���
        {
            cout << " �����λ��ֳ�ʱ" << endl;
            header.flag = FIN;
            header.sum = 0;                                            // У�����0
            header.sum = checkSum((u_short*)&header, sizeof(header)); // ����У���
            memcpy(Buffer, &header, sizeof(header));                   // ���ײ����뻺����
            sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            start = clock();
            cout << " ���ش������λ�������" << endl;
        }
    }
    cout << " �Ĵλ��ֽ��������ӶϿ���" << endl;
    return 1;
}

  

int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN serverAddr;
    SOCKET server;

    serverAddr.sin_family = AF_INET;   // ʹ��IPV4
    serverAddr.sin_port = htons(8889); // ����Ϊrouter�Ķ˿�
    serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    server = socket(AF_INET, SOCK_DGRAM, 0);
    cout << " �ɹ�����socket " << endl;
    bind(server, (SOCKADDR*)&serverAddr, sizeof(serverAddr)); // ���׽��֣��������״̬
    cout << " �������״̬���ȴ��ͻ������� " << endl;

    int len = sizeof(serverAddr);
    // ��������
    Connect(server, serverAddr, len);
    cout << " �ɹ��������ӣ����ڵȴ������ļ� " << endl;

    while (true)
    {
        char* name = new char[20];
        char* data = new char[100000000];
        int namelen = RecvMessage(server, serverAddr, len, name);
        int datalen = RecvMessage(server, serverAddr, len, data);
        string a;
        for (int i = 0; i < namelen; i++)
        {
            a = a + name[i];
        }

        cout << endl
            << " ���յ��ļ���:" << a << endl;
        ofstream fout(a.c_str(), ofstream::binary);
        cout << " ���յ��ļ�����:" << datalen << endl;
        for (int i = 0; i < datalen; i++)
        {
            fout << data[i];
        }
        fout.close();
        cout << " �ļ��ѳɹ����ص����� " << endl
            << endl;

        if (disConnect(server, serverAddr, len) == 1)
        {
            break;
        }
    }

    system("pause");
    return 0;
}