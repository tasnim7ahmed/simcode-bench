#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHttpExample");

class SimpleHttpServer : public Application
{
public:
    SimpleHttpServer() : m_socket(0) {}
    virtual ~SimpleHttpServer() {
        m_socket = 0;
    }

    void Setup(Address address) {
        m_local = address;
    }

private:
    virtual void StartApplication() {
        if (!m_socket) {
            m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
            m_socket->Bind(m_local);
            m_socket->Listen();
            m_socket->SetAcceptCallback(
                MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                MakeCallback(&SimpleHttpServer::HandleAccept, this)
            );
        }
    }

    virtual void StopApplication() {
        if (m_socket) {
            m_socket->Close();
            m_socket = 0;
        }
    }

    void HandleAccept(Ptr<Socket> s, const Address& from) {
        s->SetRecvCallback(MakeCallback(&SimpleHttpServer::HandleRead, this));
    }

    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet = socket->Recv();
        if (packet->GetSize() > 0) {
            // Send HTTP/1.0 200 OK and dummy HTML
            std::string response =
                "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: 66\r\n\r\n"
                "<html><head><title>NS-3</title></head><body>Hello World</body></html>";
            Ptr<Packet> reply = Create<Packet>((uint8_t*)response.c_str(), response.size());
            socket->Send(reply);
        }
    }

    Ptr<Socket> m_socket;
    Address m_local;
};

class SimpleHttpClient : public Application
{
public:
    SimpleHttpClient() : m_socket(0), m_send(false) {}
    virtual ~SimpleHttpClient() {
        m_socket = 0;
    }

    void Setup(Address remote, uint16_t port) {
        m_remote = remote;
        m_port = port;
    }

private:
    virtual void StartApplication() {
        if (!m_socket) {
            m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
            m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_remote).GetIpv4(), m_port));
            m_socket->SetConnectCallback(
                MakeCallback(&SimpleHttpClient::ConnectionSucceeded, this),
                MakeCallback(&SimpleHttpClient::ConnectionFailed, this)
            );
            m_socket->SetRecvCallback(MakeCallback(&SimpleHttpClient::HandleRead, this));
        }
    }

    virtual void StopApplication() {
        if (m_socket) {
            m_socket->Close();
            m_socket = 0;
        }
    }

    void ConnectionSucceeded(Ptr<Socket> socket) {
        m_send = true;
        SendRequest();
    }

    void ConnectionFailed(Ptr<Socket> socket) {
        NS_LOG_INFO("Connection failed");
    }

    void SendRequest() {
        if (m_send && m_socket) {
            std::string request = "GET / HTTP/1.0\r\nHost: server\r\n\r\n";
            Ptr<Packet> packet = Create<Packet>((uint8_t*)request.c_str(), request.size());
            m_socket->Send(packet);
        }
    }

    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        while ((packet = socket->Recv())) {
            std::string payload;
            payload.resize(packet->GetSize());
            packet->CopyData((uint8_t *)&payload[0], payload.size());
            NS_LOG_INFO("HTTP Client received response: " << payload);
        }
    }

    Ptr<Socket> m_socket;
    Address m_remote;
    uint16_t m_port;
    bool m_send;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("WifiHttpExample", LOG_LEVEL_INFO);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Mobility (2D grid, 5m x 5m)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(5.0),
        "GridWidth", UintegerValue(2),
        "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Wi-Fi (802.11b)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-http");

    // AP and station
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add(address.Assign(staDevice));
    interfaces.Add(address.Assign(apDevice));

    // Install HTTP server on node 1
    Ptr<SimpleHttpServer> httpServer = CreateObject<SimpleHttpServer>();
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), 80));
    httpServer->Setup(serverAddress);
    wifiNodes.Get(1)->AddApplication(httpServer);
    httpServer->SetStartTime(Seconds(1.0));
    httpServer->SetStopTime(Seconds(10.0));

    // Install HTTP client on node 0
    Ptr<SimpleHttpClient> httpClient = CreateObject<SimpleHttpClient>();
    Address remoteAddr(InetSocketAddress(interfaces.GetAddress(1), 80));
    httpClient->Setup(remoteAddr, 80);
    wifiNodes.Get(0)->AddApplication(httpClient);
    httpClient->SetStartTime(Seconds(2.0));
    httpClient->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}