#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void MoveAp(Ptr<Node> ap) {
    static double x_position = 0.0;
    x_position += 5.0;
    ap->GetObject<MobilityModel>()->SetPosition(Vector(x_position, 0.0, 0.0));
    Simulator::Schedule(Seconds(1.0), &MoveAp, ap);
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer staNodes;
    staNodes.Create(2);

    NodeContainer apNode;
    apNode.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Simple();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(staNodes);
    internet.Install(apNode);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(NetDeviceContainer::Create(staDevices, apDevices));

    PacketSocketHelper packetSocketHelper;
    packetSocketHelper.Install(staNodes);
    packetSocketHelper.Install(apNode);

    Ptr<Node> ap = apNode.Get(0);
    Simulator::Schedule(Seconds(1.0), &MoveAp, ap);

    Simulator::Schedule(Seconds(0.5), [](){
        Ptr<PacketSocket> sock1 = DynamicCast<PacketSocket>(NodeList::Get(0)->GetApplication(0));
        Ptr<PacketSocket> sock2 = DynamicCast<PacketSocket>(NodeList::Get(1)->GetApplication(0));
        Ptr<PacketSocket> sock3 = DynamicCast<PacketSocket>(NodeList::Get(2)->GetApplication(0));
        if(sock1 && sock2 && sock3){
            sock1->Bind(InetSocketAddress{Ipv4Address::GetAny(), 5000});
            sock2->Bind(InetSocketAddress{Ipv4Address::GetAny(), 5000});
            sock3->Bind(InetSocketAddress{Ipv4Address::GetAny(), 5000});

            Simulator::ScheduleNow([sock1, sock3]() {
                Ptr<Packet> packet = Create<Packet>(1024);
                sock1->SendTo(packet, InetSocketAddress{i.GetAddress(2), 5000});
            });

            Simulator::Schedule(Seconds(1.0), [sock2, sock3]() {
                Ptr<Packet> packet = Create<Packet>(1024);
                sock2->SendTo(packet, InetSocketAddress{i.GetAddress(2), 5000});
            });
        }

    });

    wifiPhy.EnableAsciiAll(std::string("wifi-sim.tr"));
    wifiPhy.EnablePcapAll(std::string("wifi-sim.pcap"));

    Simulator::Stop(Seconds(44.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}