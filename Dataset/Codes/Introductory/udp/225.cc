#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpVANETExample");

// Function to send UDP packets from vehicles to the RSU
void SendPacket(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(512); // 512-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(dstAddr, port));
    Simulator::Schedule(Seconds(1.5), &SendPacket, socket, dstAddr, port); // Schedule next packet
}

// Function to receive packets at the RSU
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("RSU received a packet from a vehicle!");
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpVANETExample", LOG_LEVEL_INFO);

    int numVehicles = 10;
    double simTime = 30.0;

    // Create nodes (vehicles + 1 RSU)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    NodeContainer rsu;
    rsu.Create(1);

    // Configure Wi-Fi (Ad-Hoc Mode for VANET)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for vehicular communication

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, vehicles);
    NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

    // Set up mobility (vehicles moving along a highway)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    
    for (int i = 0; i < numVehicles; i++) {
        positionAlloc->Add(Vector(i * 20.0, 0.0, 0.0)); // Vehicles spaced 20m apart
    }
    positionAlloc->Add(Vector(100.0, 50.0, 0.0)); // RSU position

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(vehicles);
    mobility.Install(rsu);

    // Install Internet stack with DSDV routing
    DsdvHelper dsdv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(dsdv);
    internet.Install(vehicles);
    internet.Install(rsu);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = address.Assign(devices);
    Ipv4InterfaceContainer rsuInterface = address.Assign(rsuDevice);

    uint16_t port = 8080; // UDP port

    // Setup UDP Server at RSU
    Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu.Get(0), UdpSocketFactory::GetTypeId());
    rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Clients in Vehicles
    for (int i = 0; i < numVehicles; i++) {
        Ptr<Socket> vehicleSocket = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, vehicleSocket, rsuInterface.GetAddress(0), port);
    }

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

