#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer csmaNode;
    csmaNode.Add(wifiApNode.Get(0));
    csmaNode.Create(1);

    // Wi-Fi Channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wi-Fi MAC and Helpers
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // STATION
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // ACCESS POINT
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // CSMA between AP and Node 3
    CsmaHelper csma;
    NetDeviceContainer csmaDevices = csma.Install(csmaNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);
    mobility.Install(csmaNode.Get(1));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);
    stack.Install(csmaNode.Get(1));

    // Assign IPs
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    // Enable routing between networks
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Server on Node 3 (csmaNode.Get(1))
    uint16_t udpPort = 4000;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(csmaNode.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on Node 1 (station)
    UdpClientHelper client(Ipv4Address("10.1.2.2"), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.03)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // UDP Forwarder on Node 2 (AP)
    // Install UDP server listening on port 5000 at AP
    UdpServerHelper apServer(5000);
    ApplicationContainer apServerApp = apServer.Install(wifiApNode.Get(0));
    apServerApp.Start(Seconds(1.0));
    apServerApp.Stop(Seconds(10.0));

    // Custom forwarding application
    class UdpForwarder : public Application
    {
      public:
        UdpForwarder() {}
        virtual ~UdpForwarder() {}
        void Setup(Address listenAddress, uint16_t listenPort, Address forwardAddress, uint16_t forwardPort)
        {
            m_listenAddress = listenAddress;
            m_listenPort = listenPort;
            m_forwardAddress = forwardAddress;
            m_forwardPort = forwardPort;
        }
      private:
        virtual void StartApplication()
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_listenPort));
            m_socket->SetRecvCallback(MakeCallback(&UdpForwarder::HandleRead, this));
            m_forwardSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        }
        virtual void StopApplication()
        {
            if (m_socket)
                m_socket->Close();
            if (m_forwardSocket)
                m_forwardSocket->Close();
        }
        void HandleRead(Ptr<Socket> socket)
        {
            while (Ptr<Packet> packet = socket->Recv())
            {
                m_forwardSocket->SendTo(packet, 0, InetSocketAddress(Ipv4Address::ConvertFrom(m_forwardAddress), m_forwardPort));
            }
        }
        Ptr<Socket> m_socket;
        Ptr<Socket> m_forwardSocket;
        Address m_listenAddress;
        uint16_t m_listenPort;
        Address m_forwardAddress;
        uint16_t m_forwardPort;
    };

    Ptr<UdpForwarder> forwarder = CreateObject<UdpForwarder>();
    // Listen on AP's CSMA interface address, port 5000; forward to csma node (Node 3) at port 4000
    Ptr<Node> ap = wifiApNode.Get(0);
    forwarder->Setup(apInterface.GetAddress(0), 5000, csmaInterfaces.GetAddress(1), 4000);
    ap->AddApplication(forwarder);
    forwarder->SetStartTime(Seconds(1.0));
    forwarder->SetStopTime(Seconds(10.0));

    // Station (Node 1) sends to AP (Node 2)'s wifi interface, port 5000
    client.SetAttribute("RemoteAddress", AddressValue(apInterface.GetAddress(0)));
    client.SetAttribute("RemotePort", UintegerValue(5000));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}