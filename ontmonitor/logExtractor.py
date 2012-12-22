# This is a draft for the BNG log extractor
# TODO Stuff that need to be done:
# - encode each command that we want to execute in such a 
# way that it's easy to find the corresponding template
# - store the output for each command in a bigtable
import os
#import textfsm

class LogExtractor:

  TABLE_HEADER = ('Interface', 'Ipv4Address', 'LinkState',
                  'Ipv6Prefix', 'Ipv6Address')

  def RunCommandOnRouter(self, host, command):
    # TODO
    print host
    print command


  def GetTemplateFileForCommand(self, command):
    print command
    # TODO

  def GetLogs(self, command):

    print command

    #table = textfsm.texttable.TextTable()

    #table.header = self.TABLE_HEADER

    # Get template and router output
    # Run command, get raw router output
    #raw = self.RunCommandOnRouter(host, command)
    # Find the template file to use
    #template = self.GetTemplateFileForCommand(command)

    # Run it through the FSM (actual textfsm methods)
    #table = textfsm.TextFSM(template)

    #re_table.ParseText(raw)

    # Display result as CSV
    #', '.join(re_tale.header)
    # Each row of the table.
    #for row in re_table:
    #    ', '.join(line)
    

def main():
  logger = LogExtractor()

  # TODO: the name of the subscriber should ber given as an argument
  logger.GetLogs('oam host-connectivity-verify subscriber \"knssks757884610001\"')
  # TODO: the ip address should be extracted from the output of the previous command
  logger.GetLogs('show router igmp hosts host 8.22.62.253 detail')

if __name__ == '__main__':
  main()

