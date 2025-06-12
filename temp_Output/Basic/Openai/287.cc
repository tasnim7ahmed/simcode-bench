#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: 1 gNB, 2 UEs
    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install Mobility
    MobilityHelper mobility;
    // gNB stationary at (25,25,3)
    Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator>();
    gnbPos->Add(Vector(25.0, 25.0, 3.0));
    mobility.SetPositionAllocator(gnbPos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    // UEs: RandomWalk2dMobilityModel within 50x50
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Z", StringValue("ns3::ConstantRandomVariable[Constant=1.5]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", TimeValue(Seconds(0.5)),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)));
    mobility.Install(ueNodes);

    // Install mmWave + EPC
    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);
    mmwaveHelper->SetSchedulerType("ns3::MmWaveFlexTtiMacScheduler");
    mmwaveHelper->Initialize();

    NetDeviceContainer gNbDevs = mmwaveHelper->InstallEnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIfaces;
    ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        mmwaveHelper->Attach(ueDevs.Get(i), gNbDevs.Get(0));
    }

    // Enable PCAP tracing
    mmwaveHelper->EnableTraces();
    epcHelper->EnablePcapInternal();

    // Install UDP server on UE 0 (port 5001)
    uint16_t serverPort = 5001;
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on UE 1: to server
    UdpClientHelper client(ueIfaces.GetAddress(0), serverPort);
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}