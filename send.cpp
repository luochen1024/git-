#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <iomanip>
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
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size);
    u_long sum = 0;
    while (count--)
    {
        sum += *buf++;
        if (sum & 0xffff0000)
        {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

struct HEADER
{
    u_short sum = 0;      // У��� 16λ
    u_short datasize = 0; // ���������ݳ��� 16λ
    // ��λ��ʹ�ú���λ��������FIN ACK SYN
    unsigned char flag = 0;
    // ��λ����������кţ�0~255��������mod
    unsigned char SEQ = 0;
    HEADER()
    {
        sum = 0;      // У��� 16λ
        datasize = 0; // ���������ݳ��� 16λ
        flag = 0;     // 8λ��ʹ�ú���λ��������FIN ACK SYN
        SEQ = 0;      // 8λ
    }
};

int Connect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen) // �������ֽ�������
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    // ��һ������
    header.flag = SYN;
    header.sum = 0; // У�����0
    // ����У���
    header.sum = checkSum((u_short*)&header, sizeof(header));
    // ������ͷ����buffer
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    else
    {
        cout << "[\033[1;31mSend\033[0m] �ɹ����͵�һ����������" << endl;
    }
    clock_t start = clock(); // ��¼���͵�һ������ʱ��

    // Ϊ�˺����ܹ��������У�����socketΪ������״̬
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);

    // �ڶ�������
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        // ��ʱ��Ҫ�ش�
        if (clock() - start > MAX_TIME) // ��ʱ�����´����һ������
        {
            cout << "[\033[1;33mInfo\033[0m] ��һ�����ֳ�ʱ" << endl;
            header.flag = SYN;
            header.sum = 0;                                            // У�����0
            header.sum = checkSum((u_short*)&header, sizeof(header)); // ����У���
            memcpy(Buffer, &header, sizeof(header));                   // ������ͷ����Buffer
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "[\033[1;33mInfo\033[0m] �Ѿ��ش�" << endl;
        }
    }

    // �ڶ������֣��յ����Խ��ն˵�ACK
    // ����У��ͼ���
    memcpy(&header, Buffer, sizeof(header));
    if (header.flag == ACK && checkSum((u_short*)&header, sizeof(header)) == 0)
    {
        cout << "[\033[1;32mReceive\033[0m] ���յ��ڶ�����������" << endl;
    }
    else
    {
        cout << "[\033[1;33mInfo\033[0m] �������ݣ�������" << endl;
        return -1;
    }

    // ���е���������
    header.flag = ACK_SYN;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header)); // ����У���
    if (sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    else
    {
        cout << "[\033[1;31mSend\033[0m] �ɹ����͵�������������" << endl;
    }
    cout << "[\033[1;33mInfo\033[0m] �������ɹ����ӣ����Է�������" << endl;
    return 1;
}

void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int& order)
{
    HEADER header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.SEQ = static_cast<unsigned char>(order);                 // ���к�
    memcpy(Buffer, &header, sizeof(header));                        // �������У��͵�header���¸�ֵ��buffer����ʱ��buffer���Է�����
    memcpy(Buffer + sizeof(header), message, len);                  // bufferΪheader+message
    header.sum = checkSum((u_short*)Buffer, sizeof(header) + len); // ����У���
    // ����
    sendto(socketClient, Buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "[\033[1;31mSend\033[0m] ������" << len << " �ֽڣ�"
        << " flag:" << int(header.flag)
        << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
    clock_t start = clock(); // ��¼����ʱ��

    // ����ack����Ϣ
    while (1)
    {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode); // ���׽ӿ�״̬��Ϊ������

        while (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            // ��ʱ�ش�
            if (clock() - start > MAX_TIME)
            {
                cout << "[\033[1;33mInfo\033[0m] �������ݳ�ʱ" << endl;
                header.datasize = len;
                header.SEQ = u_char(order); // ���к�
                header.flag = u_char(0x0);
                memcpy(Buffer, &header, sizeof(header));
                memcpy(Buffer + sizeof(header), message, sizeof(header) + len);
                u_short check = checkSum((u_short*)Buffer, sizeof(header) + len); // ����У���
                header.sum = check;
                memcpy(Buffer, &header, sizeof(header));
                sendto(socketClient, Buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen); // ����
                cout << "[\033[1;33mInfo\033[0m] ���·�������" << endl;
                start = clock(); // ��¼����ʱ��
            }
            else
                break;
        }
        memcpy(&header, Buffer, sizeof(header)); // ���������յ���Ϣ������header����ʱheader�����յ�������

        if (checkSum((u_short*)&header, sizeof(header)) == 0 && header.SEQ == u_short(order) && header.flag == ACK)
        {
            cout << "[\033[1;32mReceive\033[0m] ��ȷ���յ� - Flag:" << int(header.flag)
                << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
            break;
        }
        else
        {
            continue;
        }
    }
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode); // �Ļ�����ģʽ
}

void send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
    // ��Ҫ���͵����ݰ�����
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
    int seqnum = 0;
    cout << packagenum << endl;
    for (int i = 0; i < packagenum; i++)
    {
        send_package(socketClient, servAddr, servAddrlen,
            message + i * MAXSIZE, i == packagenum - 1 ? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum);
        seqnum++;
        if (seqnum > 255)
        {
            seqnum = seqnum - 256;
        }
    }
    // ���ͽ�����Ϣ
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    header.flag = OVER;
    header.sum = checkSum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));
    sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "[\033[1;31mSend\033[0m] ����OVER�ź�" << endl;
    clock_t start = clock();
    while (1)
    {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        // �յ���������Buffer��
        while (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            // ��ʱ���ѡ�send end�����ݰ��ش�
            if (clock() - start > MAX_TIME)
            {
                cout << "[\033[1;33mInfo\033[0m] ����OVER�źų�ʱ" << endl;
                char* Buffer = new char[sizeof(header)];
                header.flag = OVER;
                header.sum = checkSum((u_short*)&header, sizeof(header));
                memcpy(Buffer, &header, sizeof(header));
                sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                cout << "[\033[1;33mInfo\033[0m] �Ѿ��ط�OVER�ź�" << endl;
                start = clock();
            }
        }
        memcpy(&header, Buffer, sizeof(header)); // ���������յ���Ϣ����ȡBuffer�����Ϣ
        u_short check = checkSum((u_short*)&header, sizeof(header));
        // �յ������ݰ�ΪOVER���Ѿ��ɹ������ļ�
        if (header.flag == OVER && check == 0)
        {
            cout << "[\033[1;33mInfo\033[0m] �Է��ѳɹ������ļ�" << endl;
            break;
        }
        else
        {
            continue;
        }
    }
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode); // �Ļ�����ģʽ
}

int disConnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    u_short sum;

    // ���е�һ�λ���
    header.flag = FIN;
    header.sum = 0;                                            // У�����0
    header.sum = checkSum((u_short*)&header, sizeof(header)); // ����У���
    memcpy(Buffer, &header, sizeof(header));                   // ���ײ����뻺����
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    else
    {
        cout << "[\033[1;31mSend\033[0m] �ɹ����͵�һ�λ�������" << endl;
    }
    clock_t start = clock(); // ��¼���͵�һ�λ���ʱ��

    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode); // FIONBIOΪ�������1/��ֹ0�׽ӿ�s�ķ�����1/����0ģʽ��

    // ���յڶ��λ���
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        // ��ʱ�����´����һ�λ���
        if (clock() - start > MAX_TIME)
        {
            cout << "[\033[1;33mInfo\033[0m] ��һ�λ��ֳ�ʱ" << endl;
            header.flag = FIN;
            header.sum = 0;                                            // У�����0
            header.sum = checkSum((u_short*)&header, sizeof(header)); // ����У���
            memcpy(Buffer, &header, sizeof(header));                   // ���ײ����뻺����
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "[\033[1;33mInfo\033[0m] ���ش���һ�λ�������" << endl;
        }
    }

    // ����У��ͼ���
    memcpy(&header, Buffer, sizeof(header));
    if (header.flag == ACK && checkSum((u_short*)&header, sizeof(header) == 0))
    {
        cout << "[\033[1;32mReceive\033[0m] ���յ��ڶ��λ�������" << endl;
    }
    else
    {
        cout << "[\033[1;33mInfo\033[0m] �������ݣ�������" << endl;
        return -1;
    }

    // ���е����λ���
    start = clock(); // ��¼�ڶ��λ��ַ���ʱ��

    // ���յ����λ���
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        // ���͵ڶ��λ��ֵȴ������λ��ֹ����г�ʱ���ش��ڶ��λ���
        if (clock() - start > MAX_TIME)
        {
            cout << "[\033[1;33mInfo\033[0m] �ڶ��λ��ֳ�ʱ " << endl;
            header.flag = ACK;
            header.sum = 0;
            header.sum = checkSum((u_short*)&header, sizeof(header));
            memcpy(Buffer, &header, sizeof(header));
            if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
            {
                return -1;
            }
            cout << "[\033[1;33mInfo\033[0m] ���ش��ڶ��λ������� " << endl;
        }
    }
    // �����յ��ĵ����λ���
    HEADER temp1;
    memcpy(&temp1, Buffer, sizeof(header));
    if (temp1.flag == FIN_ACK && checkSum((u_short*)&temp1, sizeof(temp1) == 0))
    {
        cout << "[\033[1;32mReceive\033[0m] ���յ������λ������� " << endl;
    }
    else
    {
        cout << "[\033[1;33mInfo\033[0m] �������ݣ�������" << endl;
        return -1;
    }

    // ���͵��Ĵλ�����Ϣ
    header.flag = FIN_ACK;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        cout << "[\033[1;33mInfo\033[0m] ���Ĵλ��ַ���ʧ�� " << endl;
        return -1;
    }
    else
    {
        cout << "[\033[1;31mSend\033[0m] �ɹ����͵��Ĵλ������� " << endl;
    }
    cout << "[\033[1;33mInfo\033[0m] �Ĵλ��ֽ��������ӶϿ��� " << endl;
    return 1;
}
int main()
{
    cout << MAX_TIME << endl;

    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN serverAddr;
    SOCKET server;

    serverAddr.sin_family = AF_INET; // ʹ��IPV4
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    server = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(serverAddr);

    // ��������
    if (Connect(server, serverAddr, len) == -1)
    {
        return 0;
    }

    bool flag = true;
    while (true)
    {
        cout << endl
            << "ѡ����Ҫ���еĲ���" << endl
            << "1. �˳�" << endl;
        if (flag)
        {
            cout << "2. �����ļ�" << endl;
            flag = !flag;
        }
        else
        {
            cout << "2. ���������ļ�" << endl;
        }

        int choice = {};

        cin >> choice;
        cout << endl;
        if (choice == 1)
            break;
        else
        {

            // ��ȡ�ļ����ݵ�buffer
            string inputFile; // ϣ��������ļ�����
            cout << "������ϣ��������ļ�����" << endl;
            cin >> inputFile;
            ifstream fileIN(inputFile.c_str(), ifstream::binary); // �Զ����Ʒ�ʽ���ļ�
            char* buffer = new char[100000000];                   // �ļ�����
            int i = 0;
            unsigned char temp = fileIN.get();
            while (fileIN)
            {
                buffer[i++] = temp;
                temp = fileIN.get();
            }
            fileIN.close();

            // �����ļ���
            send(server, serverAddr, len, (char*)(inputFile.c_str()), inputFile.length());
            clock_t start1 = clock();
            // �����ļ����ݣ���buffer�
            send(server, serverAddr, len, buffer, i);
            clock_t end1 = clock();

            cout << "[\033[1;36mOut\033[0m] ������ʱ��Ϊ:" << (end1 - start1) / CLOCKS_PER_SEC << "s" << endl;
            cout << "[\033[1;36mOut\033[0m] ������Ϊ:" << fixed << setprecision(2) << (((double)i) / ((end1 - start1) / CLOCKS_PER_SEC)) << "byte/s" << endl;
        }
    }

    disConnect(server, serverAddr, len);
    system("pause");
    return 0;
}