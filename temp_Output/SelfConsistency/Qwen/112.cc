#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixSimulation");

class PingApp : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("PingApp")
      .SetParent<Application> ()
      .AddConstructor<PingApp> ()
      .AddAttribute ("Remote", "The remote node's IPv6 address",
                     Ipv6AddressValue (),
                     MakeIpv6AddressAccessor (&PingApp::m_remote),
                     MakeIpv6AddressChecker ())
    ;
    return tid;
  }

  PingApp () {}
  virtual ~PingApp () {}

private:
  virtual void StartApplication (void)
  {
    TypeId tid = TypeId::LookupByName ("ns3::Icmpv6Echo");
    ObjectFactory factory;
    factory.SetTypeId (tid);
    Ptr<Socket> socket = Socket::CreateSocket (GetNode (), tid);
    m_socket = socket;

    uint8_t ttl = 255;
    socket->SetIpTtl (ttl);

    Icmpv6Echo echo;
    echo.SetId (0x1234);
    echo.SetSequence (1);
    Ptr<Packet> p = Create<Packet> (64);
    p->AddHeader (echo);

    m_socket->SendTo (p, 0, Inet6SocketAddress (m_remote, 0));
    NS_LOG_INFO ("Sent ICMPv6 Echo Request to " << m_remote);
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      }
  }

  Ptr<Socket> m_socket;
  Ipv6Address m_remote;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("RadvdTwoPrefixSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer n0nR;
  n0nR.Create (2);
  NodeContainer n1 = NodeContainer::GetGlobal ()->Get (1); // Reuse existing node for R
  NodeContainer nRn1 = NodeContainer (n0nR.Get (1), n1);

  // Install CSMA on n0-R link
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d0dR = csma.Install (n0nR);

  // Install PointToPoint on R-n1 link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer dRd1 = p2p.Install (nRn1);

  // Install Internet stack
  InternetStackHelper internet;
  internet.SetIpv4StackInstall (false);
  internet.InstallAll ();

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;

  // Subnet 2001:1::/64 and 2001:ABCD::/64 for n0 side
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0iR = ipv6.Assign (d0dR);
  i0iR.SetForwarding (1, true);
  i0iR.SetDefaultRouteInAllNodes ();

  // Assign second prefix to router interface
  ipv6.SetBase (Ipv6Address ("2001:ABCD::"), Ipv6Prefix (64));
  ipv6.AssignWithoutAddress (d0dR);
  i0iR.Get (1)->AddAddress (Ipv6InterfaceAddress (Ipv6Address ("2001:ABCD::1"), Ipv6Prefix (64)));

  // Subnet 2001:2::/64 for n1 side
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iRi1 = ipv6.Assign (dRd1);
  iRi1.SetForwarding (0, true);
  iRi1.SetDefaultRouteInAllNodes ();

  // Enable RA on n0's subnet
  Ipv6StaticRoutingHelper routingHelper;
  Ptr<Ipv6StaticRouting> RrtrStaticRouting = routingHelper.GetStaticRouting (n0nR.Get (1)->GetObject<Ipv6> ());
  RrtrStaticRouting->SetDefaultRoute (iRi1.GetAddress (0), 1);

  // Configure RAdv for n0's subnet
  Ipv6InterfaceContainer radvInterfaces;
  radvInterfaces.Add (i0iR, 1);
  Ipv6Address radvAddr = i0iR.GetAddress (1, 0);
  Ipv6Address network1 = Ipv6Address ("2001:1::");
  Ipv6Address network2 = Ipv6Address ("2001:ABCD::");

  Ipv6RadvdHelper radvdHelper;
  radvdHelper.AddAnnouncedPrefix (radvInterfaces.Get (0)->GetDevice (), network1, 64);
  radvdHelper.AddAnnouncedPrefix (radvInterfaces.Get (0)->GetDevice (), network2, 64);
  radvdHelper.SetMaxRtrAdvInterval (Seconds (10));
  radvdHelper.SetMinRtrAdvInterval (Seconds (5));
  radvdHelper.SetLinkMtu (1500);

  ApplicationContainer radvdApps = radvdHelper.Install (n0nR.Get (1));
  radvdApps.Start (Seconds (1.0));
  radvdApps.Stop (Seconds (10.0));

  // Configure ping application
  Ipv6Address target = iRi1.GetAddress (1, 0);
  PacketSinkHelper sink ("ns3::UdpSocketFactory", Inet6SocketAddress (Ipv6Address::GetAllNodesMulticast (), 9));
  ApplicationContainer sinkApps = sink.Install (n1.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  PingApp pingApp;
  pingApp.SetRemote (target);
  ApplicationContainer pingApps = ApplicationContainer (&pingApp);
  pingApps.Install (n0nR.Get (0));
  pingApps.Start (Seconds (2.0));
  pingApps.Stop (Seconds (2.1));

  // Setup tracing
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("radvd-two-prefix.tr"));
  p2p.EnableAsciiAll (ascii.CreateFileStream ("radvd-two-prefix.tr"));

  csma.EnablePcapAll ("radvd-two-prefix");
  p2p.EnablePcapAll ("radvd-two-prefix");

  // Print IP addresses
  NS_LOG_INFO ("IPv6 Addresses:");
  for (uint32_t i = 0; i < i0iR.GetN (); ++i)
    {
      NS_LOG_INFO ("Node " << i << ": " << i0iR.GetAddress (i, 0));
    }
  for (uint32_t i = 0; i < iRi1.GetN (); ++i)
    {
      NS_LOG_INFO ("Node " << (i + 2) << ": " << iRi1.GetAddress (i, 0));
    }

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}