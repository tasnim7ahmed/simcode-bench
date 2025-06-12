#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("MmWaveHelper", LOG_LEVEL_INFO);
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);

    // Create nodes: one gNB and two UEs
    NodeContainer gnbs;
    gnbs.Create(1);

    NodeContainer ues;
    ues.Create(2);

    // Mobility for UEs - Random Walk in 50x50 area
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ues);

    // Install the 5G NR stack using MmWave module
    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetSchedulerType("ns3::MmWaveFlexTtiMacScheduler");

    NetDeviceContainer gnbdDevices;
    gnbdDevices = mmwaveHelper->InstallGnbDevice(gnbs, Config::GetRootNamespaceObject<Node>());

    NetDeviceContainer ueDevices;
    ueDevices = mmwaveHelper->InstallUeDevice(ues, Config::GetRootNamespaceObject<Node>());

    // Attach UEs to the gNodeB
    for (uint32_t i = 0; i < ues.GetN(); ++i) {
        mmwaveHelper->AttachToClosestEnb(ueDevices.Get(i), gnbdDevices.Get(0));
    }

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ues);
    internet.Install(gnbs);

    // Assign IP addresses
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueInterfaces;
    ueInterfaces = ipAddrs.Assign(ueDevices);

    // Set up TCP server on UE 1 (listening on port 8080)
    uint16_t serverPort = 8080;
    PacketSinkHelper packetSink("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer sinkApp = packetSink.Install(ues.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP client on UE 0
    OnOffHelper client("ns3::TcpSocketFactory", 
                       InetSocketAddress(ueInterfaces.GetAddress(1), serverPort));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(ues.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}