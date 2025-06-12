#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpUAVNetwork");

// Function to send UDP packets from UAVs to the GCS
void SendPacket(Ptr<Socket> socket, Ipv4Address gcsAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(512); // 512-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(gcsAddr, port));
    Simulator::Schedule(Seconds(2.0), &SendPacket, socket, gcsAddr, port); // Schedule next packet
}

// Function to receive packets at the GCS
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("GCS received a packet from a UAV!");
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpUAVNetwork", LOG_LEVEL_INFO);

    int numUAVs = 5;
    double simTime = 60.0;

    // Create nodes (UAVs + 1 Ground Control Station)
    NodeContainer uavs;
    uavs.Create(numUAVs);

    NodeContainer gcs;
    gcs.Create(1);

    // Configure Wi-Fi (Ad-Hoc Mode for UAV Network)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a); // 802.11a for high-speed UAV communication

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer uavDevices = wifi.Install(phy, mac, uavs);
    NetDeviceContainer gcsDevice = wifi.Install(phy, mac, gcs);

    // Set up 3D mobility model for UAVs
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    
    for (int i = 0; i < numUAVs; i++) {
        positionAlloc->Add(Vector(i * 50.0, i * 50.0, 100.0 + i * 10.0)); // UAVs at different altitudes
    }
    positionAlloc->Add(Vector(250.0, 250.0, 0.0)); // GCS at ground level

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "PositionAllocator", PointerValue(positionAlloc));
    mobility.Install(uavs);
    mobility.Install(gcs);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(uavs);
    internet.Install(gcs);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer uavInterfaces = address.Assign(uavDevices);
    Ipv4InterfaceContainer gcsInterface = address.Assign(gcsDevice);

    uint16_t port = 7070; // UDP port

    // Setup UDP Server at GCS
    Ptr<Socket> gcsSocket = Socket::CreateSocket(gcs.Get(0), UdpSocketFactory::GetTypeId());
    gcsSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    gcsSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Clients in UAVs
    for (int i = 0; i < numUAVs; i++) {
        Ptr<Socket> uavSocket = Socket::CreateSocket(uavs.Get(i), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0 + i * 1.0), &SendPacket, uavSocket, gcsInterface.GetAddress(0), port);
    }

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

