#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void SendPacket (Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval) {
    socket->SendTo (Create<Packet> (packetSize), 0, address);
    Simulator::Schedule (interval, &SendPacket, socket, address, packetSize, interval);
}

int main(int argc, char *argv[])
{
    uint32_t packetSize = 200;
    double simulationTime = 10.0;
    Time interPacketInterval = Seconds(1.0);

    NodeContainer nodes;
    nodes.Create(2); // 0: Vehicle, 1: RSU

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));     // Vehicle initial position
    positionAlloc->Add (Vector (100.0, 0.0, 0.0));   // RSU position
    mobility.SetPositionAllocator (positionAlloc);

    // Vehicle mobility: moves from (0,0) towards (200,0)
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(0));
    nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity (Vector (10.0, 0.0, 0.0)); // 10 m/s

    // RSU: fixed
    MobilityHelper rsuMobility;
    rsuMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    rsuMobility.Install(nodes.Get(1));

    // 802.11p (WAVE) setup
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());
    NqosWaveMacHelper mac = NqosWaveMacHelper::Default ();
    Wifi80211pHelper wave = Wifi80211pHelper::Default ();
    wave.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                  "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = wave.Install (phy, mac, nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // UDP server (on RSU)
    uint16_t port = 4000;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get(1));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (simulationTime));

    // Socket on Vehicle to send to RSU
    Ptr<Socket> srcSocket = Socket::CreateSocket (nodes.Get(0), UdpSocketFactory::GetTypeId ());
    Address destAddress = InetSocketAddress (interfaces.GetAddress(1), port);

    Simulator::ScheduleWithContext (srcSocket->GetNode ()->GetId (), Seconds (1.0),
            &SendPacket, srcSocket, destAddress, packetSize, interPacketInterval);

    // Cleanup and run
    Simulator::Stop (Seconds(simulationTime+1));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}