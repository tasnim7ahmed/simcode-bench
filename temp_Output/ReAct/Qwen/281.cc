#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simDuration = 10.0;
    uint32_t numUes = 3;
    uint16_t udpPort = 9;
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);

    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::AnrEnabled", BooleanValue(false));
    Config::SetDefault("ns3::LteHelper::UsePdschForCqiGeneration", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(0.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobilityUe.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    for (uint32_t u = 0; u < numUes; ++u) {
        lteHelper->Attach(ueDevs.Get(u), enbDevs.Get(0));
    }

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < enbNodes.GetN(); ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
        serverApps.Add(sinkHelper.Install(enbNodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    ApplicationContainer clientApps;
    for (uint32_t u = 0; u < numUes; ++u) {
        InetSocketAddress dstAddr(ueIpIfaces.GetAddress(u), udpPort);
        dstAddr.SetTos(0xb8); // AC_BE
        OnOffHelper onoff("ns3::UdpSocketFactory", dstAddr);
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("NumberOfPackets", UintegerValue(10));
        clientApps.Add(onoff.Install(ueNodes.Get(u)));
        clientApps.Start(Seconds(1.0 + u * 0.1)); // staggered start
        clientApps.Stop(Seconds(simDuration));
    }

    lteHelper->EnableTraces();
    epcHelper->EnableMmeTraces();
    epcHelper->EnableSgwTraces();
    epcHelper->EnablePgwTraces();
    internet.EnableAsciiIpv4Internal("lte_udp_randomwalk_mobility.pcap", PcapHelper::ND_STREAM_DEFAULT, true, true);

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}