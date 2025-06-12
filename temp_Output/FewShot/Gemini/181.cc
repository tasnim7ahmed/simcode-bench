#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-mac-module.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-helper.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETSimulation");

void ReceivePacket(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    double time = Simulator::Now().GetSeconds();
    std::cout << time << "s: Received one packet!" << std::endl;
}

static void InstallApplication(Ptr<Node> node, Ipv4Address address, uint16_t port) {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSock = Socket::CreateSocket(node, tid);
    InetSocketAddress local = InetSocketAddress(address, port);
    recvSock->Bind(local);
    recvSock->SetRecvCallback(MakeCallback(&ReceivePacket));
}

int main(int argc, char *argv[]) {
    bool enable_pcap = false;
    bool enable_netanim = true;

    CommandLine cmd;
    cmd.AddValue("pcap", "Enable PCAP tracing", enable_pcap);
    cmd.AddValue("netanim", "Enable NetAnim visualization", enable_netanim);
    cmd.Parse(argc, argv);

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set initial position and velocity
    Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    mob0->SetVelocity(Vector(10.0, 0.0, 0.0));

    Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    mob1->SetPosition(Vector(20.0, 0.0, 0.0));
    mob1->SetVelocity(Vector(10.0, 0.0, 0.0));


    // Configure Wave
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    QosWaveMacHelper mac = QosWaveMacHelper::Default();
    WaveHelper waveHelper;
    NetDeviceContainer devices = waveHelper.Install(nodes, phy, mac);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    // Install Applications
    uint16_t port = 9;
    InstallApplication(nodes.Get(1), i.GetAddress(1), port);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sendSock = Socket::CreateSocket(nodes.Get(0), tid);
    InetSocketAddress remote = InetSocketAddress(i.GetAddress(1), port);
    sendSock->Connect(remote);

    Simulator::ScheduleWithContext(nodes.Get(0)->GetId(), Seconds(1.0), [sendSock]() {
        Ptr<Packet> packet = Create<Packet>(100);
        sendSock->Send(packet);
    });

    Simulator::ScheduleWithContext(nodes.Get(0)->GetId(), Seconds(2.0), [sendSock]() {
        Ptr<Packet> packet = Create<Packet>(100);
        sendSock->Send(packet);
    });
    
    // Enable PCAP Tracing
    if (enable_pcap) {
        phy.EnablePcapAll("vanet");
    }

    // Enable NetAnim
    if (enable_netanim) {
        AnimationInterface anim("vanet.xml");
    }

    // Run Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}