#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpVANETExample");

// Function to send UDP packets from vehicles to the RSU
void SendPacket(Ptr<Socket> socket, Ipv4Address rsuAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(256); // 256-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(rsuAddr, port));
    Simulator::Schedule(Seconds(3.0), &SendPacket, socket, rsuAddr, port); // Schedule next packet
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
    double simTime = 80.0;

    // Create nodes (Vehicles + 1 Roadside Unit)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    NodeContainer rsu;
    rsu.Create(1);

    // Configure Wi-Fi (Ad-Hoc Mode for VANET)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for VANET communication

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer vehicleDevices = wifi.Install(phy, mac, vehicles);
    NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

    // Set up mobility model for vehicles (simulate road movement)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::GaussMarkovMobilityModel",
                              "Bounds", BoxValue(Box(0, 500, 0, 500, 0, 0)),
                              "TimeStep", TimeValue(Seconds(1.0)),
                              "Alpha", DoubleValue(0.85),
                              "MeanVelocity", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"),
                              "MeanDirection", StringValue("ns3::UniformRandomVariable[Min=0|Max=360]"),
                              "MeanPitch", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    mobility.Install(vehicles);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(250.0, 250.0, 0.0)); // RSU fixed at center

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsu);

    // Install Internet stack with DSR routing
    DsrMainHelper dsrMain;
    DsrHelper dsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(dsr);
    internet.Install(vehicles);
    internet.Install(rsu);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = address.Assign(vehicleDevices);
    Ipv4InterfaceContainer rsuInterface = address.Assign(rsuDevice);

    uint16_t port = 8080; // UDP port

    // Setup UDP Server at RSU
    Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu.Get(0), UdpSocketFactory::GetTypeId());
    rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Clients in Vehicles
    for (int i = 0; i < numVehicles; i++) {
        Ptr<Socket> vehicleSocket = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0 + i * 1.0), &SendPacket, vehicleSocket, rsuInterface.GetAddress(0), port);
    }

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

