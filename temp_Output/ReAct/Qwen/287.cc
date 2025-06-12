#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGSimulation");

int main(int argc, char *argv[]) {
    uint16_t simTime = 10;
    uint16_t port = 5001;
    std::string phyMode("OfdmRate24Mbps");
    double interval = 0.01; // seconds (10ms)
    uint32_t packetSize = 1024;

    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(1000000));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(Seconds(interval)));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(packetSize));

    NodeContainer gnbNode;
    gnbNode.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // gNB at origin
    positionAlloc->Add(Vector(10.0, 10.0, 0.0)); // UE1
    positionAlloc->Add(Vector(20.0, 20.0, 0.0)); // UE2

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);

    mmwave::MmWaveHelper mmwaveHelper;
    mmwaveHelper.Initialize();
    NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnbDevice(gnbNode, ueNodes);
    NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes, gnbNode);

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueInterfaces = ipv4.Assign(ueDevs);
    Ipv4InterfaceContainer gnbInterfaces = ipv4.Assign(gnbDevs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(ueInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue((uint32_t)(simTime / interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    mmwaveHelper.EnableTraces();
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("simulation.tr");
    mmwaveHelper.EnablePhyTraces(stream);
    mmwaveHelper.EnableMacTraces(stream);

    for (auto dev : ueDevs) {
        dev->GetChannel()->GetObject<PointToPointChannel>()->EnablePcapAll("pcap_output");
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}