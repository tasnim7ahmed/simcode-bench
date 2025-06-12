#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet-socket.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMobilityPacketSocketExample");

void MoveApCallback(Ptr<Node> apNode, uint32_t seconds, uint32_t duration) {
    if (seconds > duration) return;
    Ptr<MobilityModel> mobility = apNode->GetObject<MobilityModel>();
    Vector pos = mobility->GetPosition();
    pos.x += 5.0;
    mobility->SetPosition(pos);
    Simulator::Schedule(Seconds(1.0), &MoveApCallback, apNode, seconds + 1, duration);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("WifiMobilityPacketSocketExample", LOG_LEVEL_INFO);

    // Nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Wifi Channel/PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // MAC & SSID
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", 
        "DataMode", StringValue("ErpOfdmRate6Mbps"), 
        "ControlMode", StringValue("ErpOfdmRate6Mbps"));
    Ssid ssid = Ssid("ns3-ssid");
    WifiMacHelper mac;
    // Sta
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);
    // AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    // AP mobility install separately
    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.SetPositionAllocator("ns3::ListPositionAllocator",
                                    "PositionList", VectorValue({Vector(20.0, 0.0, 0.0)}));
    apMobility.Install(wifiApNode);

    // Packet Socket
    PacketSocketHelper packetSocket;
    packetSocket.Install(wifiStaNodes);
    packetSocket.Install(wifiApNode);

    // Tracing
    wifi.EnableMacRx("mac-rx.tr");
    wifi.EnableMacTx("mac-tx.tr");
    phy.EnablePcap("phy", staDevices);
    phy.EnablePcap("phy", apDevices);
    phy.EnablePcapAll("phy-all");
    phy.EnableAsciiAll(std::cout);

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", 
                    MakeCallback([](Ptr<const Packet> p) {
                        NS_LOG_INFO("MAC TX " << p->GetSize() << "B");
                    }));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", 
                    MakeCallback([](Ptr<const Packet> p) {
                        NS_LOG_INFO("MAC RX " << p->GetSize() << "B");
                    }));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", 
        MakeCallback([](Time start, Time duration, WifiPhyState state){
            NS_LOG_INFO("PHY State " << state << " Start " << start.GetSeconds() << " Duration " << duration.GetSeconds());
        }));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxOk",
        MakeCallback([](Ptr<const Packet> p, double snr, WifiMode mode, enum WifiPreamble preamble){
            NS_LOG_INFO("PHY RX OK " << p->GetSize() << "B SNR=" << snr);
        }));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxError",
        MakeCallback([](Ptr<const Packet> p, double snr){
            NS_LOG_INFO("PHY RX ERR " << p->GetSize() << "B SNR=" << snr);
        }));

    // Application - send UDP packets from Sta 0 to Sta 1 -- use PacketSocket
    Address sta1Socket;
    PacketSocketAddress socketSta1;
    socketSta1.SetSingleDevice(staDevices.Get(1)->GetIfIndex());
    socketSta1.SetPhysicalAddress(staDevices.Get(1)->GetAddress());
    socketSta1.SetProtocol(1);
    sta1Socket = socketSta1;

    Ptr<Socket> senderSocket = Socket::CreateSocket(wifiStaNodes.Get(0), PacketSocketFactory::GetTypeId());
    senderSocket->Bind();

    Ptr<Socket> receiverSocket = Socket::CreateSocket(wifiStaNodes.Get(1), PacketSocketFactory::GetTypeId());
    receiverSocket->Bind(socketSta1);

    // Data rate 500kbps, packet size 1024B, interval
    uint32_t pktSize = 1024;
    double dataRateBps = 500000.0;
    double interval = pktSize * 8 / dataRateBps; // seconds per packet

    class MySenderApp : public Application
    {
    public:
        MySenderApp () {}
        void Setup (Ptr<Socket> socket, Address dst, uint32_t pktSize, double interval)
        {
            m_socket = socket;
            m_dst = dst;
            m_pktSize = pktSize;
            m_interval = interval;
            m_running = false;
        }
    private:
        virtual void StartApplication ()
        {
            m_running = true;
            SendPacket();
        }
        virtual void StopApplication ()
        {
            m_running = false;
            if (m_sendEvent.IsRunning())
                Simulator::Cancel(m_sendEvent);
        }
        void SendPacket ()
        {
            if (!m_running) return;
            Ptr<Packet> pkt = Create<Packet>(m_pktSize);
            m_socket->SendTo(pkt, 0, m_dst);
            m_sendEvent = Simulator::Schedule(Seconds(m_interval), &MySenderApp::SendPacket, this);
        }
        Ptr<Socket> m_socket;
        Address m_dst;
        uint32_t m_pktSize;
        double m_interval;
        EventId m_sendEvent;
        bool m_running;
    };

    Ptr<MySenderApp> app = CreateObject<MySenderApp> ();
    app->Setup (senderSocket, sta1Socket, pktSize, interval);
    wifiStaNodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(0.5));
    app->SetStopTime(Seconds(44.0));

    // Mobility event: move AP along x axis every second
    Simulator::Schedule(Seconds(1.0), &MoveApCallback, wifiApNode.Get(0), 1, 43);

    Simulator::Stop(Seconds(44.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}