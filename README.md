# raspi-finance-ncurses

## fzy
git@github.com:jhawthorn/fzy.git

## requires cjson
brew install cjson
yay -S cjson

## jq to get accounts
curl -s -X GET 'http://localhost:8080/account/select/active' | jq '.[] | .accountNameOwner' | tr -d '"'
