#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mmwave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWave5GSimulation");

int main(int argc, char *argv[]) {
    uint16_t gNbNum = 1;
    uint16_t ueNum = 2;
    double simTime = 10.0;
    double interPacketInterval = 0.01; // seconds
    uint32_t packetSize = 1024;
    uint16_t port = 5001;

    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(0xFFFFFFFF));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(Seconds(interPacketInterval)));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(packetSize));

    NodeContainer gnbNodes;
    gnbNodes.Create(gNbNum);

    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // gNB at origin

    MobilityHelper mobilityGnb;
    mobilityGnb.SetPositionAllocator(positionAlloc);
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator<RandomRectanglePositionAllocator>("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNodes);

    InternetStackHelper internet;
    internet.InstallAll();

    PointToPointEpcHelper epcHelper;
    MmWaveHelper mmwaveHelper;

    mmwaveHelper.SetSchedulerType("ns3::MmWaveFlexTtiMacScheduler");
    mmwaveHelper.SetHarqEnabled(true);

    NetDeviceContainer enbDevs = mmwaveHelper.InstallEnbDevice(gnbNodes, epcHelper.GetEpcEnbApplication());
    NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes, epcHelper.GetEpcUeNas());

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ueIpIfaces = ipv4.Assign(ueDevs);

    epcHelper.AttachUes(ueDevs);

    TypeId tid = TypeId::LookupByName("ns3::UdpServer");
    ObjectFactory serverFactory;
    serverFactory.SetTypeId(tid);
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < ueNum; ++i) {
        ApplicationContainer app = serverFactory.Create<Application>();
        ueNodes.Get(i)->AddApplication(app);
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simTime));
    }

    UdpClientHelper client(ueIpIfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(0.1));
    clientApp.Stop(Seconds(simTime));

    mmwaveHelper.EnableTraces();
    mmwaveHelper.EnableDlDataTraces();
    mmwaveHelper.EnableUlDataTraces();

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mmwave-5g.tr");
    mmwaveHelper.EnableAsciiIpv4Mobility(stream);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}